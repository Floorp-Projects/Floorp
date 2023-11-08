/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This program acts as a linker wrapper. Its executable name is meant
// to be that of a linker, and it will find the next linker with the same
// name in $PATH. However, if for some reason the next linker cannot be
// found this way, the caller may pass its path via the --real-linker
// option.
//
// More in-depth background on https://glandium.org/blog/?p=4297

#include "relrhack.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <spawn.h>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

class CantSwapSections : public std::runtime_error {
 public:
  CantSwapSections(const char* what) : std::runtime_error(what) {}
};

template <int bits>
struct Elf {};

#define ELF(bits)                        \
  template <>                            \
  struct Elf<bits> {                     \
    using Ehdr = Elf##bits##_Ehdr;       \
    using Phdr = Elf##bits##_Phdr;       \
    using Shdr = Elf##bits##_Shdr;       \
    using Dyn = Elf##bits##_Dyn;         \
    using Addr = Elf##bits##_Addr;       \
    using Word = Elf##bits##_Word;       \
    using Off = Elf##bits##_Off;         \
    using Verneed = Elf##bits##_Verneed; \
    using Vernaux = Elf##bits##_Vernaux; \
  }

ELF(32);
ELF(64);

template <int bits>
struct RelR : public Elf<bits> {
  using Elf_Ehdr = typename Elf<bits>::Ehdr;
  using Elf_Phdr = typename Elf<bits>::Phdr;
  using Elf_Shdr = typename Elf<bits>::Shdr;
  using Elf_Dyn = typename Elf<bits>::Dyn;
  using Elf_Addr = typename Elf<bits>::Addr;
  using Elf_Word = typename Elf<bits>::Word;
  using Elf_Off = typename Elf<bits>::Off;
  using Elf_Verneed = typename Elf<bits>::Verneed;
  using Elf_Vernaux = typename Elf<bits>::Vernaux;

#define TAG_NAME(t) \
  { t, #t }
  class DynInfo {
   public:
    using Tag = decltype(Elf_Dyn::d_tag);
    using Value = decltype(Elf_Dyn::d_un.d_val);
    bool is_wanted(Tag tag) const { return tag_names.count(tag); }
    void insert(off_t offset, Tag tag, Value val) {
      data[tag] = std::make_pair(offset, val);
    }
    off_t offset(Tag tag) const { return data.at(tag).first; }
    bool contains(Tag tag) const { return data.count(tag); }
    Value& operator[](Tag tag) {
      if (!is_wanted(tag)) {
        std::stringstream msg;
        msg << "Tag 0x" << std::hex << tag << " is not in DynInfo::tag_names";
        throw std::runtime_error(msg.str());
      }
      return data[tag].second;
    }
    const char* name(Tag tag) const { return tag_names.at(tag); }

   private:
    std::unordered_map<Tag, std::pair<off_t, Value>> data;

    const std::unordered_map<Tag, const char*> tag_names = {
        TAG_NAME(DT_JMPREL),  TAG_NAME(DT_PLTRELSZ), TAG_NAME(DT_RELR),
        TAG_NAME(DT_RELRENT), TAG_NAME(DT_RELRSZ),   TAG_NAME(DT_RELA),
        TAG_NAME(DT_RELASZ),  TAG_NAME(DT_RELAENT),  TAG_NAME(DT_REL),
        TAG_NAME(DT_RELSZ),   TAG_NAME(DT_RELENT),   TAG_NAME(DT_STRTAB),
        TAG_NAME(DT_STRSZ),   TAG_NAME(DT_VERNEED),  TAG_NAME(DT_VERNEEDNUM),
    };
  };

  // Translate a virtual address into an offset in the file based on the program
  // headers' PT_LOAD.
  static Elf_Addr get_offset(const std::vector<Elf_Phdr>& phdr, Elf_Addr addr) {
    for (const auto& p : phdr) {
      if (p.p_type == PT_LOAD && addr >= p.p_vaddr &&
          addr < p.p_vaddr + p.p_filesz) {
        return addr - (p.p_vaddr - p.p_paddr);
      }
    }
    return 0;
  }

  static bool hack(std::fstream& f);
};

template <typename T>
T read_one_at(std::istream& in, off_t pos) {
  T result;
  in.seekg(pos, std::ios::beg);
  in.read(reinterpret_cast<char*>(&result), sizeof(T));
  return result;
}

template <typename T>
std::vector<T> read_vector_at(std::istream& in, off_t pos, size_t num) {
  std::vector<T> result(num);
  in.seekg(pos, std::ios::beg);
  in.read(reinterpret_cast<char*>(result.data()), num * sizeof(T));
  return result;
}

void write_at(std::ostream& out, off_t pos, const char* buf, size_t len) {
  out.seekp(pos, std::ios::beg);
  out.write(buf, len);
}

template <typename T>
void write_one_at(std::ostream& out, off_t pos, const T& data) {
  write_at(out, pos, reinterpret_cast<const char*>(&data), sizeof(T));
}

template <typename T>
void write_vector_at(std::ostream& out, off_t pos, const std::vector<T>& vec) {
  write_at(out, pos, reinterpret_cast<const char*>(&vec.front()),
           vec.size() * sizeof(T));
}

template <int bits>
bool RelR<bits>::hack(std::fstream& f) {
  auto ehdr = read_one_at<Elf_Ehdr>(f, 0);
  if (ehdr.e_phentsize != sizeof(Elf_Phdr)) {
    throw std::runtime_error("Invalid ELF?");
  }
  auto phdr = read_vector_at<Elf_Phdr>(f, ehdr.e_phoff, ehdr.e_phnum);
  const auto& dyn_phdr =
      std::find_if(phdr.begin(), phdr.end(),
                   [](const auto& p) { return p.p_type == PT_DYNAMIC; });
  if (dyn_phdr == phdr.end()) {
    return false;
  }
  if (dyn_phdr->p_filesz % sizeof(Elf_Dyn)) {
    throw std::runtime_error("Invalid ELF?");
  }
  auto dyn = read_vector_at<Elf_Dyn>(f, dyn_phdr->p_offset,
                                     dyn_phdr->p_filesz / sizeof(Elf_Dyn));
  off_t dyn_offset = dyn_phdr->p_offset;
  DynInfo dyn_info;
  for (const auto& d : dyn) {
    if (d.d_tag == DT_NULL) {
      break;
    }

    if (dyn_info.is_wanted(d.d_tag)) {
      if (dyn_info.contains(d.d_tag)) {
        std::stringstream msg;
        msg << dyn_info.name(d.d_tag) << " appears twice?";
        throw std::runtime_error(msg.str());
      }
      dyn_info.insert(dyn_offset, d.d_tag, d.d_un.d_val);
    }
    dyn_offset += sizeof(Elf_Dyn);
  }

  // Find the location and size of the SHT_RELR section, which contains the
  // packed-relative-relocs.
  Elf_Addr relr_off =
      dyn_info.contains(DT_RELR) ? get_offset(phdr, dyn_info[DT_RELR]) : 0;
  Elf_Off relrsz = dyn_info[DT_RELRSZ];
  const decltype(Elf_Dyn::d_tag) rel_tags[3][2] = {
      {DT_REL, DT_RELA}, {DT_RELSZ, DT_RELASZ}, {DT_RELENT, DT_RELAENT}};
  for (const auto& [rel_tag, rela_tag] : rel_tags) {
    if (dyn_info.contains(rel_tag) && dyn_info.contains(rela_tag)) {
      std::stringstream msg;
      msg << "Both " << dyn_info.name(rel_tag) << " and "
          << dyn_info.name(rela_tag) << " appear?";
      throw std::runtime_error(msg.str());
    }
  }
  Elf_Off relent =
      dyn_info.contains(DT_RELENT) ? dyn_info[DT_RELENT] : dyn_info[DT_RELAENT];

  // Estimate the size of the unpacked relative relocations corresponding
  // to the SHT_RELR section.
  auto relr = read_vector_at<Elf_Addr>(f, relr_off, relrsz / sizeof(Elf_Addr));
  size_t relocs = 0;
  for (const auto& entry : relr) {
    if ((entry & 1) == 0) {
      // LSB is 0, this is a pointer for a single relocation.
      relocs++;
    } else {
      // LSB is 1, remaining bits are a bitmap. Each bit represents a
      // relocation.
      relocs += __builtin_popcount(entry) - 1;
    }
  }
  // If the packed relocations + some overhead (we pick 4K arbitrarily, the
  // real size would require digging into the section sizes of the injected
  // .o file, which is not worth the error) is larger than the estimated
  // unpacked relocations, we'll just relink without packed relocations.
  if (relocs * relent < relrsz + 4096) {
    return false;
  }

  // Change DT_RELR* tags to add DT_RELRHACK_BIT.
  for (const auto tag : {DT_RELR, DT_RELRSZ, DT_RELRENT}) {
    write_one_at(f, dyn_info.offset(tag), tag | DT_RELRHACK_BIT);
  }

  bool is_glibc = false;

  if (dyn_info.contains(DT_VERNEEDNUM) && dyn_info.contains(DT_VERNEED) &&
      dyn_info.contains(DT_STRSZ) && dyn_info.contains(DT_STRTAB)) {
    // Scan SHT_VERNEED for the GLIBC_ABI_DT_RELR version on the libc
    // library.
    Elf_Addr verneed_off = get_offset(phdr, dyn_info[DT_VERNEED]);
    Elf_Off verneednum = dyn_info[DT_VERNEEDNUM];
    // SHT_STRTAB section, which contains the string table for, among other
    // things, the symbol versions in the SHT_VERNEED section.
    auto strtab = read_vector_at<char>(f, get_offset(phdr, dyn_info[DT_STRTAB]),
                                       dyn_info[DT_STRSZ]);
    // Guarantee a nul character at the end of the string table.
    strtab.push_back(0);
    while (verneednum--) {
      auto verneed = read_one_at<Elf_Verneed>(f, verneed_off);
      if (std::string_view{"libc.so.6"} == &strtab.at(verneed.vn_file)) {
        is_glibc = true;
        Elf_Addr vernaux_off = verneed_off + verneed.vn_aux;
        Elf_Addr relr = 0;
        Elf_Vernaux reuse;
        for (auto n = 0; n < verneed.vn_cnt; n++) {
          auto vernaux = read_one_at<Elf_Vernaux>(f, vernaux_off);
          if (std::string_view{"GLIBC_ABI_DT_RELR"} ==
              &strtab.at(vernaux.vna_name)) {
            relr = vernaux_off;
          } else {
            reuse = vernaux;
          }
          vernaux_off += vernaux.vna_next;
        }
        // In the case where we do have the GLIBC_ABI_DT_RELR version, we
        // need to edit the binary to make the following changes:
        // - Remove the GLIBC_ABI_DT_RELR version, we replace it with an
        // arbitrary other version entry, which is simpler than completely
        // removing it. We need to remove it because older versions of glibc
        // don't have the version (after all, that's why the symbol version
        // is there in the first place, to avoid running against older versions
        // of glibc that don't support packed relocations).
        // - Alter the DT_RELR* tags in the dynamic section, so that they
        // are not recognized by ld.so, because, while all versions of ld.so
        // ignore tags they don't know, glibc's ld.so versions that support
        // packed relocations don't want to load a binary that has DT_RELR*
        // tags but *not* a dependency on the GLIBC_ABI_DT_RELR version.
        if (relr) {
          // Don't overwrite vn_aux.
          write_at(f, relr, reinterpret_cast<char*>(&reuse),
                   sizeof(reuse) - sizeof(Elf_Word));
        }
      }
      verneed_off += verneed.vn_next;
    }
  }

  // Location of the .rel.plt section.
  Elf_Addr jmprel = dyn_info.contains(DT_JMPREL) ? dyn_info[DT_JMPREL] : 0;
  if (is_glibc) {
#ifndef MOZ_STDCXX_COMPAT
    try {
#endif
      // ld.so in glibc 2.16 to 2.23 expects .rel.plt to strictly follow
      // .rel.dyn. (https://sourceware.org/bugzilla/show_bug.cgi?id=14341)
      // BFD ld places .relr.dyn after .rel.plt, so this works fine, but lld
      // places it between both sections, which doesn't work out for us. In that
      // case, we want to swap .relr.dyn and .rel.plt.
      Elf_Addr rel_end = dyn_info.contains(DT_REL)
                             ? (dyn_info[DT_REL] + dyn_info[DT_RELSZ])
                             : (dyn_info[DT_RELA] + dyn_info[DT_RELASZ]);
      if (dyn_info.contains(DT_JMPREL) && dyn_info[DT_PLTRELSZ] &&
          dyn_info[DT_JMPREL] != rel_end) {
        if (dyn_info[DT_RELR] != rel_end) {
          throw CantSwapSections("RELR section doesn't follow REL/RELA?");
        }
        if (dyn_info[DT_JMPREL] != dyn_info[DT_RELR] + dyn_info[DT_RELRSZ]) {
          throw CantSwapSections("PLT REL/RELA doesn't follow RELR?");
        }
        auto plt_rel = read_vector_at<char>(
            f, get_offset(phdr, dyn_info[DT_JMPREL]), dyn_info[DT_PLTRELSZ]);
        // Write the content of both sections swapped, and adjust the
        // corresponding PT_DYNAMIC entries.
        write_vector_at(f, relr_off, plt_rel);
        write_vector_at(f, relr_off + plt_rel.size(), relr);
        dyn_info[DT_JMPREL] = rel_end;
        dyn_info[DT_RELR] = rel_end + plt_rel.size();
        for (const auto tag : {DT_JMPREL, DT_RELR}) {
          write_one_at(f, dyn_info.offset(tag) + sizeof(typename DynInfo::Tag),
                       dyn_info[tag]);
        }
      }
#ifndef MOZ_STDCXX_COMPAT
    } catch (const CantSwapSections& err) {
      // When binary compatibility with older libstdc++/glibc is not enabled, we
      // only emit a warning about why swapping the sections is not happening.
      std::cerr << "WARNING: " << err.what() << std::endl;
    }
#endif
  }

  off_t shdr_offset = ehdr.e_shoff;
  auto shdr = read_vector_at<Elf_Shdr>(f, ehdr.e_shoff, ehdr.e_shnum);
  for (auto& s : shdr) {
    // Some tools don't like sections of types they don't know, so change
    // SHT_RELR, which might be unknown on older systems, to SHT_PROGBITS.
    if (s.sh_type == SHT_RELR) {
      s.sh_type = SHT_PROGBITS;
      // If DT_RELR has been adjusted to swap with DT_JMPREL, also adjust
      // the corresponding SHT_RELR section header.
      if (s.sh_addr != dyn_info[DT_RELR]) {
        s.sh_offset += dyn_info[DT_RELR] - s.sh_addr;
        s.sh_addr = dyn_info[DT_RELR];
      }
      write_one_at(f, shdr_offset, s);
    } else if (jmprel && (s.sh_addr == jmprel) &&
               (s.sh_addr != dyn_info[DT_JMPREL])) {
      // If DT_JMPREL has been adjusted to swap with DT_RELR, also adjust
      // the corresponding section header.
      s.sh_offset -= s.sh_addr - dyn_info[DT_JMPREL];
      s.sh_addr = dyn_info[DT_JMPREL];
      write_one_at(f, shdr_offset, s);
    }
    shdr_offset += sizeof(Elf_Shdr);
  }
  return true;
}

std::vector<std::string> get_path() {
  std::vector<std::string> result;
  std::stringstream stream{std::getenv("PATH")};
  std::string item;

  while (std::getline(stream, item, ':')) {
    result.push_back(std::move(item));
  }

  return result;
}

std::optional<fs::path> next_program(fs::path& this_program,
                                     std::optional<fs::path>& program) {
  auto program_name = program ? *program : this_program.filename();
  for (const auto& dir : get_path()) {
    auto path = fs::path(dir) / program_name;
    auto status = fs::status(path);
    if ((status.type() == fs::file_type::regular) &&
        ((status.permissions() & fs::perms::owner_exec) ==
         fs::perms::owner_exec) &&
        !fs::equivalent(path, this_program))
      return path;
  }
  return std::nullopt;
}

unsigned char get_elf_class(unsigned char (&e_ident)[EI_NIDENT]) {
  if (std::string_view{reinterpret_cast<char*>(e_ident), SELFMAG} !=
      std::string_view{ELFMAG, SELFMAG}) {
    throw std::runtime_error("Not ELF?");
  }
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  if (e_ident[EI_DATA] != ELFDATA2LSB) {
    throw std::runtime_error("Not Little Endian ELF?");
  }
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  if (e_ident[EI_DATA] != ELFDATA2MSB) {
    throw std::runtime_error("Not Big Endian ELF?");
  }
#else
#  error Unknown byte order.
#endif
  if (e_ident[EI_VERSION] != 1) {
    throw std::runtime_error("Not ELF version 1?");
  }
  auto elf_class = e_ident[EI_CLASS];
  if (elf_class != ELFCLASS32 && elf_class != ELFCLASS64) {
    throw std::runtime_error("Not 32 or 64-bits ELF?");
  }
  return elf_class;
}

unsigned char get_elf_class(std::istream& in) {
  unsigned char e_ident[EI_NIDENT];
  in.read(reinterpret_cast<char*>(e_ident), sizeof(e_ident));
  return get_elf_class(e_ident);
}

uint16_t get_elf_machine(std::istream& in) {
  // As far as e_machine is concerned, both Elf32_Ehdr and Elf64_Ehdr are equal.
  Elf32_Ehdr ehdr;
  in.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
  // get_elf_class will throw exceptions for the cases we don't handle.
  get_elf_class(ehdr.e_ident);
  return ehdr.e_machine;
}

int run_command(std::vector<const char*>& args) {
  pid_t child_pid;
  if (posix_spawn(&child_pid, args[0], nullptr, nullptr,
                  const_cast<char* const*>(args.data()), environ) != 0) {
    throw std::runtime_error("posix_spawn failed");
  }

  int status;
  waitpid(child_pid, &status, 0);
  return WEXITSTATUS(status);
}

int main(int argc, char* argv[]) {
  auto this_program = fs::absolute(argv[0]);

  std::vector<const char*> args;

  int i, crti = 0;
  std::optional<fs::path> output = std::nullopt;
  std::optional<fs::path> real_linker = std::nullopt;
  bool shared = false;
  bool is_android = false;
  uint16_t elf_machine = EM_NONE;
  // Scan argv in order to prepare the following:
  // - get the output file. That's the file we may need to adjust.
  // - get the --real-linker if one was passed.
  // - detect whether we're linking a shared library or something else. As of
  // now, only shared libraries are handled. Technically speaking, programs
  // could be handled as well, but for the purpose of Firefox, that actually
  // doesn't work because programs contain a memory allocator that ends up
  // being called before the injected code has any chance to apply relocations,
  // and the allocator itself needs the relocations to have been applied.
  // - detect the position of crti.o so that we can inject our own object
  // right after it, and also to detect the machine type to pick the right
  // object to inject.
  //
  // At the same time, we also construct a new list of arguments, with
  // --real-linker filtered out. We'll later inject arguments in that list.
  for (i = 1, argv++; i < argc && *argv; argv++, i++) {
    std::string_view arg{*argv};
    if (arg == "-shared") {
      shared = true;
    } else if (arg == "-o") {
      args.push_back(*(argv++));
      ++i;
      output = *argv;
    } else if (arg == "--real-linker") {
      ++i;
      real_linker = *(++argv);
      continue;
    } else if (elf_machine == EM_NONE) {
      auto filename = fs::path(arg).filename();
      if (filename == "crti.o" || filename == "crtbegin_so.o") {
        is_android = (filename == "crtbegin_so.o");
        crti = i;
        std::fstream f{std::string(arg), f.binary | f.in};
        f.exceptions(f.failbit);
        elf_machine = get_elf_machine(f);
      }
    }
    args.push_back(*argv);
  }

  if (!output) {
    std::cerr << "Could not determine output file." << std::endl;
    return 1;
  }

  if (!crti) {
    std::cerr << "Could not find CRT object on the command line." << std::endl;
    return 1;
  }

  if (!real_linker || !real_linker->has_parent_path()) {
    auto linker = next_program(this_program, real_linker);
    if (!linker) {
      std::cerr << "Could not find next "
                << (real_linker ? real_linker->filename()
                                : this_program.filename())
                << std::endl;
      return 1;
    }
    real_linker = linker;
  }
  args.insert(args.begin(), real_linker->c_str());
  args.push_back(nullptr);

  std::string stem;
  switch (elf_machine) {
    case EM_NONE:
      std::cerr << "Could not determine target machine type." << std::endl;
      return 1;
    case EM_386:
      stem = "x86";
      break;
    case EM_X86_64:
      stem = "x86_64";
      break;
    case EM_ARM:
      stem = "arm";
      break;
    case EM_AARCH64:
      stem = "aarch64";
      break;
    default:
      std::cerr << "Unsupported target machine type." << std::endl;
      return 1;
  }
  if (is_android) {
    stem += "-android";
  }

  if (shared) {
    std::vector<const char*> hacked_args(args);
    auto inject = this_program.parent_path() / "inject" / (stem + ".o");
    hacked_args.insert(hacked_args.begin() + crti + 1, inject.c_str());
    hacked_args.insert(hacked_args.end() - 1, {"-z", "pack-relative-relocs",
                                               "-init=_relrhack_wrap_init"});
    int status = run_command(hacked_args);
    if (status) {
      return status;
    }
    bool hacked = false;
    try {
      std::fstream f{*output, f.binary | f.in | f.out};
      f.exceptions(f.failbit);
      auto elf_class = get_elf_class(f);
      f.seekg(0, std::ios::beg);
      if (elf_class == ELFCLASS32) {
        hacked = RelR<32>::hack(f);
      } else if (elf_class == ELFCLASS64) {
        hacked = RelR<64>::hack(f);
      }
    } catch (const std::runtime_error& err) {
      std::cerr << "Failed to hack " << output->string() << ": " << err.what()
                << std::endl;
      return 1;
    }
    if (hacked) {
      return 0;
    }
  }

  return run_command(args);
}
