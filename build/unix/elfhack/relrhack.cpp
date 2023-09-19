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
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

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
  // Find the location and size of several sections from the .dynamic section
  // contents:
  // - SHT_RELR section, which contains the packed-relative-relocs.
  // - SHT_VERNEED section, which contains the symbol versions needed.
  // - SHT_STRTAB section, which contains the string table for, among other
  // things, those symbol versions.
  // At the same time, we also change DT_RELR* tags to add DT_RELRHACK_BIT.
  auto dyn = read_vector_at<Elf_Dyn>(f, dyn_phdr->p_offset,
                                     dyn_phdr->p_filesz / sizeof(Elf_Dyn));
  off_t dyn_offset = dyn_phdr->p_offset;
  Elf_Addr strtab_off = 0, verneed_off = 0, relr_off = 0;
  Elf_Off strsz = 0, verneednum = 0, relrsz = 0, relent = 0;
  std::vector<std::pair<off_t, Elf_Off>> relr_tags;
  for (const auto& d : dyn) {
    if (d.d_tag == DT_NULL) {
      break;
    }
    switch (d.d_tag) {
      case DT_RELR:
      case DT_RELRENT:
      case DT_RELRSZ:
        if (d.d_tag == DT_RELR) {
          if (relr_off) {
            throw std::runtime_error("DT_RELR appears twice?");
          }
          relr_off = get_offset(phdr, d.d_un.d_ptr);
        } else if (d.d_tag == DT_RELRSZ) {
          if (relrsz) {
            throw std::runtime_error("DT_RELRSZ appears twice?");
          }
          relrsz = d.d_un.d_val;
        }
        relr_tags.push_back({dyn_offset, d.d_tag | DT_RELRHACK_BIT});
        break;
      case DT_RELAENT:
      case DT_RELENT:
        relent = d.d_un.d_val;
        break;
      case DT_STRTAB:
        strtab_off = get_offset(phdr, d.d_un.d_ptr);
        break;
      case DT_STRSZ:
        strsz = d.d_un.d_val;
        break;
      case DT_VERNEED:
        verneed_off = get_offset(phdr, d.d_un.d_ptr);
        break;
      case DT_VERNEEDNUM:
        verneednum = d.d_un.d_val;
        break;
    }
    dyn_offset += sizeof(Elf_Dyn);
  }

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

  if (verneednum && verneed_off && strsz && strtab_off) {
    // Scan SHT_VERNEED for the GLIBC_ABI_DT_RELR version on the libc
    // library.
    auto strtab = read_vector_at<char>(f, strtab_off, strsz);
    // Guarantee a nul character at the end of the string table.
    strtab.push_back(0);
    while (verneednum--) {
      auto verneed = read_one_at<Elf_Verneed>(f, verneed_off);
      if (std::string_view{"libc.so.6"} == &strtab.at(verneed.vn_file)) {
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
          f.seekg(relr, std::ios::beg);
          // Don't overwrite vn_aux.
          f.write(reinterpret_cast<char*>(&reuse),
                  sizeof(reuse) - sizeof(Elf_Word));
          for (const auto& [offset, tag] : relr_tags) {
            f.seekg(offset, std::ios::beg);
            f.write(reinterpret_cast<const char*>(&tag), sizeof(tag));
          }
        }
      }
      verneed_off += verneed.vn_next;
    }
  }
  off_t shdr_offset = ehdr.e_shoff + offsetof(Elf_Shdr, sh_type);
  auto shdr = read_vector_at<Elf_Shdr>(f, ehdr.e_shoff, ehdr.e_shnum);
  for (const auto& s : shdr) {
    // Some tools don't like sections of types they don't know, so change
    // SHT_RELR, which might be unknown on older systems, to SHT_PROGBITS.
    if (s.sh_type == SHT_RELR) {
      Elf_Word progbits = SHT_PROGBITS;
      f.seekg(shdr_offset, std::ios::beg);
      f.write(reinterpret_cast<const char*>(&progbits), sizeof(progbits));
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
