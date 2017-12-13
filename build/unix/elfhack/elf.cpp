/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#undef NDEBUG
#include <cstring>
#include <assert.h>
#include "elfxx.h"

template <class endian, typename R, typename T>
void Elf_Ehdr_Traits::swap(T &t, R &r)
{
    memcpy(r.e_ident, t.e_ident, sizeof(r.e_ident));
    r.e_type = endian::swap(t.e_type);
    r.e_machine = endian::swap(t.e_machine);
    r.e_version = endian::swap(t.e_version);
    r.e_entry = endian::swap(t.e_entry);
    r.e_phoff = endian::swap(t.e_phoff);
    r.e_shoff = endian::swap(t.e_shoff);
    r.e_flags = endian::swap(t.e_flags);
    r.e_ehsize = endian::swap(t.e_ehsize);
    r.e_phentsize = endian::swap(t.e_phentsize);
    r.e_phnum = endian::swap(t.e_phnum);
    r.e_shentsize = endian::swap(t.e_shentsize);
    r.e_shnum = endian::swap(t.e_shnum);
    r.e_shstrndx = endian::swap(t.e_shstrndx);
}

template <class endian, typename R, typename T>
void Elf_Phdr_Traits::swap(T &t, R &r)
{
    r.p_type = endian::swap(t.p_type);
    r.p_offset = endian::swap(t.p_offset);
    r.p_vaddr = endian::swap(t.p_vaddr);
    r.p_paddr = endian::swap(t.p_paddr);
    r.p_filesz = endian::swap(t.p_filesz);
    r.p_memsz = endian::swap(t.p_memsz);
    r.p_flags = endian::swap(t.p_flags);
    r.p_align = endian::swap(t.p_align);
}

template <class endian, typename R, typename T>
void Elf_Shdr_Traits::swap(T &t, R &r)
{
    r.sh_name = endian::swap(t.sh_name);
    r.sh_type = endian::swap(t.sh_type);
    r.sh_flags = endian::swap(t.sh_flags);
    r.sh_addr = endian::swap(t.sh_addr);
    r.sh_offset = endian::swap(t.sh_offset);
    r.sh_size = endian::swap(t.sh_size);
    r.sh_link = endian::swap(t.sh_link);
    r.sh_info = endian::swap(t.sh_info);
    r.sh_addralign = endian::swap(t.sh_addralign);
    r.sh_entsize = endian::swap(t.sh_entsize);
}

template <class endian, typename R, typename T>
void Elf_Dyn_Traits::swap(T &t, R &r)
{
    r.d_tag = endian::swap(t.d_tag);
    r.d_un.d_val = endian::swap(t.d_un.d_val);
}

template <class endian, typename R, typename T>
void Elf_Sym_Traits::swap(T &t, R &r)
{
    r.st_name = endian::swap(t.st_name);
    r.st_value = endian::swap(t.st_value);
    r.st_size = endian::swap(t.st_size);
    r.st_info = t.st_info;
    r.st_other = t.st_other;
    r.st_shndx = endian::swap(t.st_shndx);
}

template <class endian>
struct _Rel_info {
    static inline void swap(Elf32_Word &t, Elf32_Word &r) { r = endian::swap(t); }
    static inline void swap(Elf64_Xword &t, Elf64_Xword &r) { r = endian::swap(t); }
    static inline void swap(Elf64_Xword &t, Elf32_Word &r) {
        r = endian::swap(ELF32_R_INFO(ELF64_R_SYM(t), ELF64_R_TYPE(t)));
    }
    static inline void swap(Elf32_Word &t, Elf64_Xword &r) {
        r = endian::swap(ELF64_R_INFO(ELF32_R_SYM(t), ELF32_R_TYPE(t)));
    }
};

template <class endian, typename R, typename T>
void Elf_Rel_Traits::swap(T &t, R &r)
{
    r.r_offset = endian::swap(t.r_offset);
    _Rel_info<endian>::swap(t.r_info, r.r_info);
}

template <class endian, typename R, typename T>
void Elf_Rela_Traits::swap(T &t, R &r)
{
    r.r_offset = endian::swap(t.r_offset);
    _Rel_info<endian>::swap(t.r_info, r.r_info);
    r.r_addend = endian::swap(t.r_addend);
}

static const Elf32_Shdr null32_section =
    { 0, SHT_NULL, 0, 0, 0, 0, SHN_UNDEF, 0, 0, 0 };

Elf_Shdr null_section(null32_section);

Elf_Ehdr::Elf_Ehdr(std::ifstream &file, char ei_class, char ei_data)
: serializable<Elf_Ehdr_Traits>(file, ei_class, ei_data),
  ElfSection(null_section, nullptr, nullptr)
{
    shdr.sh_size = Elf_Ehdr::size(ei_class);
}

Elf::Elf(std::ifstream &file)
{
    if (!file.is_open())
        throw std::runtime_error("Error opening file");

    file.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);
    // Read ELF magic number and identification information
    char e_ident[EI_VERSION];
    file.seekg(0);
    file.read(e_ident, sizeof(e_ident));
    file.seekg(0);
    ehdr = new Elf_Ehdr(file, e_ident[EI_CLASS], e_ident[EI_DATA]);

    // ELFOSABI_LINUX is kept unsupported because I haven't looked whether
    // STB_GNU_UNIQUE or STT_GNU_IFUNC would need special casing.
    if ((ehdr->e_ident[EI_OSABI] != ELFOSABI_NONE) && (ehdr->e_ident[EI_ABIVERSION] != 0))
        throw std::runtime_error("unsupported ELF ABI");

    if (ehdr->e_version != 1)
        throw std::runtime_error("unsupported ELF version");

    // Sanity checks
    if (ehdr->e_shnum == 0)
        throw std::runtime_error("sstripped ELF files aren't supported");

    if (ehdr->e_ehsize != Elf_Ehdr::size(e_ident[EI_CLASS]))
        throw std::runtime_error("unsupported ELF inconsistency: ehdr.e_ehsize != sizeof(ehdr)");

    if (ehdr->e_shentsize != Elf_Shdr::size(e_ident[EI_CLASS]))
        throw std::runtime_error("unsupported ELF inconsistency: ehdr.e_shentsize != sizeof(shdr)");

    if (ehdr->e_phnum == 0) {
        if (ehdr->e_phoff != 0)
            throw std::runtime_error("unsupported ELF inconsistency: e_phnum == 0 && e_phoff != 0");
        if (ehdr->e_phentsize != 0)
            throw std::runtime_error("unsupported ELF inconsistency: e_phnum == 0 && e_phentsize != 0");
    } else if (ehdr->e_phoff != ehdr->e_ehsize)
        throw std::runtime_error("unsupported ELF inconsistency: ehdr->e_phoff != ehdr->e_ehsize");
    else if (ehdr->e_phentsize != Elf_Phdr::size(e_ident[EI_CLASS]))
        throw std::runtime_error("unsupported ELF inconsistency: ehdr->e_phentsize != sizeof(phdr)");

    // Read section headers
    Elf_Shdr **shdr = new Elf_Shdr *[ehdr->e_shnum];
    file.seekg(ehdr->e_shoff);
    for (int i = 0; i < ehdr->e_shnum; i++)
        shdr[i] = new Elf_Shdr(file, e_ident[EI_CLASS], e_ident[EI_DATA]);

    // Sanity check in section header for index 0
    if ((shdr[0]->sh_name != 0) || (shdr[0]->sh_type != SHT_NULL) ||
        (shdr[0]->sh_flags != 0) || (shdr[0]->sh_addr != 0) ||
        (shdr[0]->sh_offset != 0) || (shdr[0]->sh_size != 0) ||
        (shdr[0]->sh_link != SHN_UNDEF) || (shdr[0]->sh_info != 0) ||
        (shdr[0]->sh_addralign != 0) || (shdr[0]->sh_entsize != 0))
        throw std::runtime_error("Section header for index 0 contains unsupported values");

    if ((shdr[ehdr->e_shstrndx]->sh_link != 0) || (shdr[ehdr->e_shstrndx]->sh_info != 0))
        throw std::runtime_error("unsupported ELF content: string table with sh_link != 0 || sh_info != 0");

    // Store these temporarily
    tmp_shdr = shdr;
    tmp_file = &file;

    // Fill sections list
    sections = new ElfSection *[ehdr->e_shnum];
    for (int i = 0; i < ehdr->e_shnum; i++)
        sections[i] = nullptr;
    for (int i = 1; i < ehdr->e_shnum; i++) {
        if (sections[i] != nullptr)
            continue;
        getSection(i);
    }
    Elf_Shdr s;
    s.sh_name = 0;
    s.sh_type = SHT_NULL;
    s.sh_flags = 0;
    s.sh_addr = 0;
    s.sh_offset = ehdr->e_shoff;
    s.sh_entsize = Elf_Shdr::size(e_ident[EI_CLASS]);
    s.sh_size = s.sh_entsize * ehdr->e_shnum;
    s.sh_link = 0;
    s.sh_info = 0;
    s.sh_addralign = (e_ident[EI_CLASS] == ELFCLASS32) ? 4 : 8;
    shdr_section = new ElfSection(s, nullptr, nullptr);

    // Fake section for program headers
    s.sh_offset = ehdr->e_phoff;
    s.sh_addr = ehdr->e_phoff;
    s.sh_entsize = Elf_Phdr::size(e_ident[EI_CLASS]);
    s.sh_size = s.sh_entsize * ehdr->e_phnum;
    phdr_section = new ElfSection(s, nullptr, nullptr);

    phdr_section->insertAfter(ehdr, false);

    sections[1]->insertAfter(phdr_section, false);
    for (int i = 2; i < ehdr->e_shnum; i++) {
        // TODO: this should be done in a better way
        if ((shdr_section->getPrevious() == nullptr) && (shdr[i]->sh_offset > ehdr->e_shoff)) {
            shdr_section->insertAfter(sections[i - 1], false);
            sections[i]->insertAfter(shdr_section, false);
        } else
            sections[i]->insertAfter(sections[i - 1], false);
    }
    if (shdr_section->getPrevious() == nullptr)
        shdr_section->insertAfter(sections[ehdr->e_shnum - 1], false);

    tmp_file = nullptr;
    tmp_shdr = nullptr;
    for (int i = 0; i < ehdr->e_shnum; i++)
        delete shdr[i];
    delete[] shdr;

    eh_shstrndx = (ElfStrtab_Section *)sections[ehdr->e_shstrndx];

    // Skip reading program headers if there aren't any
    if (ehdr->e_phnum == 0)
        return;

    bool adjusted_phdr_section = false;
    // Read program headers
    file.seekg(ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf_Phdr phdr(file, e_ident[EI_CLASS], e_ident[EI_DATA]);
        if (phdr.p_type == PT_LOAD) {
            // Default alignment for PT_LOAD on x86-64 prevents elfhack from
            // doing anything useful. However, the system doesn't actually
            // require such a big alignment, so in order for elfhack to work
            // efficiently, reduce alignment when it's originally the default
            // one.
            if ((ehdr->e_machine == EM_X86_64) && (phdr.p_align == 0x200000))
              phdr.p_align = 0x1000;
        }
        ElfSegment *segment = new ElfSegment(&phdr);
        // Some segments aren't entirely filled (if at all) by sections
        // For those, we use fake sections
        if ((phdr.p_type == PT_LOAD) && (phdr.p_offset == 0)) {
            // Use a fake section for ehdr and phdr
            ehdr->getShdr().sh_addr = phdr.p_vaddr;
            if (!adjusted_phdr_section) {
                phdr_section->getShdr().sh_addr += phdr.p_vaddr;
                adjusted_phdr_section = true;
            }
            segment->addSection(ehdr);
            segment->addSection(phdr_section);
        }
        if (phdr.p_type == PT_PHDR) {
            if (!adjusted_phdr_section) {
                phdr_section->getShdr().sh_addr = phdr.p_vaddr;
                adjusted_phdr_section = true;
            }
            segment->addSection(phdr_section);
        }
        for (int j = 1; j < ehdr->e_shnum; j++)
            if (phdr.contains(sections[j]))
                segment->addSection(sections[j]);
        // Make sure that our view of segments corresponds to the original
        // ELF file.
        // GNU gold likes to start some segments before the first section
        // they contain. https://sourceware.org/bugzilla/show_bug.cgi?id=19392
        unsigned int gold_adjustment = segment->getAddr() - phdr.p_vaddr;
        assert(segment->getFileSize() == phdr.p_filesz - gold_adjustment);
        // gold makes TLS segments end on an aligned virtual address, even
        // when the underlying section ends before that, while bfd ld
        // doesn't. It's fine if we don't keep that alignment.
        unsigned int memsize = segment->getMemSize();
        if (phdr.p_type == PT_TLS && memsize != phdr.p_memsz) {
            unsigned int align = segment->getAlign();
            memsize = (memsize + align - 1) & ~(align - 1);
        }
        assert(memsize == phdr.p_memsz - gold_adjustment);
        segments.push_back(segment);
    }

    new (&eh_entry) ElfLocation(ehdr->e_entry, this);
}

Elf::~Elf()
{
    for (std::vector<ElfSegment *>::iterator seg = segments.begin(); seg != segments.end(); seg++)
        delete *seg;
    delete[] sections;
    ElfSection *section = ehdr;
    while (section != nullptr) {
        ElfSection *next = section->getNext();
        delete section;
        section = next;
    }
}

// TODO: This shouldn't fail after inserting sections
ElfSection *Elf::getSection(int index)
{
    if ((index < -1) || (index >= ehdr->e_shnum))
        throw std::runtime_error("Section index out of bounds");
    if (index == -1)
        index = ehdr->e_shstrndx; // TODO: should be fixed to use the actual current number
    // Special case: the section at index 0 is void
    if (index == 0)
        return nullptr;
    // Infinite recursion guard
    if (sections[index] == (ElfSection *)this)
        return nullptr;
    if (sections[index] == nullptr) {
        sections[index] = (ElfSection *)this;
        switch (tmp_shdr[index]->sh_type) {
        case SHT_DYNAMIC:
            sections[index] = new ElfDynamic_Section(*tmp_shdr[index], tmp_file, this);
            break;
        case SHT_REL:
            sections[index] = new ElfRel_Section<Elf_Rel>(*tmp_shdr[index], tmp_file, this);
            break;
        case SHT_RELA:
            sections[index] = new ElfRel_Section<Elf_Rela>(*tmp_shdr[index], tmp_file, this);
            break;
        case SHT_DYNSYM:
        case SHT_SYMTAB:
            sections[index] = new ElfSymtab_Section(*tmp_shdr[index], tmp_file, this);
            break;
        case SHT_STRTAB:
            sections[index] = new ElfStrtab_Section(*tmp_shdr[index], tmp_file, this);
            break;
        default:
            sections[index] = new ElfSection(*tmp_shdr[index], tmp_file, this);
        }
    }
    return sections[index];
}

ElfSection *Elf::getSectionAt(unsigned int offset)
{
    for (int i = 1; i < ehdr->e_shnum; i++) {
        ElfSection *section = getSection(i);
        if ((section != nullptr) && (section->getFlags() & SHF_ALLOC) && !(section->getFlags() & SHF_TLS) &&
            (offset >= section->getAddr()) && (offset < section->getAddr() + section->getSize()))
            return section;
    }
    return nullptr;
}

ElfSegment *Elf::getSegmentByType(unsigned int type, ElfSegment *last)
{
    std::vector<ElfSegment *>::iterator seg;
    if (last) {
        seg = std::find(segments.begin(), segments.end(), last);
        ++seg;
    } else
        seg = segments.begin();
    for (; seg != segments.end(); seg++)
        if ((*seg)->getType() == type)
            return *seg;
    return nullptr;
}

void Elf::removeSegment(ElfSegment *segment)
{
    if (!segment)
        return;
    std::vector<ElfSegment *>::iterator seg;
    seg = std::find(segments.begin(), segments.end(), segment);
    if (seg == segments.end())
        return;
    segment->clear();
    segments.erase(seg);
}

ElfDynamic_Section *Elf::getDynSection()
{
    for (std::vector<ElfSegment *>::iterator seg = segments.begin(); seg != segments.end(); seg++)
        if (((*seg)->getType() == PT_DYNAMIC) && ((*seg)->getFirstSection() != nullptr) &&
            (*seg)->getFirstSection()->getType() == SHT_DYNAMIC)
            return (ElfDynamic_Section *)(*seg)->getFirstSection();

    return nullptr;
}

void Elf::normalize()
{
    // fixup section headers sh_name; TODO: that should be done by sections
    // themselves
    for (ElfSection *section = ehdr; section != nullptr; section = section->getNext()) {
        if (section->getIndex() == 0)
            continue;
        else
            ehdr->e_shnum = section->getIndex() + 1;
        section->getShdr().sh_name = eh_shstrndx->getStrIndex(section->getName());
    }
    ehdr->markDirty();
    // Check segments consistency
    int i = 0;
    for (std::vector<ElfSegment *>::iterator seg = segments.begin(); seg != segments.end(); seg++, i++) {
        std::list<ElfSection *>::iterator it = (*seg)->begin();
        for (ElfSection *last = *(it++); it != (*seg)->end(); last = *(it++)) {
            if (((*it)->getType() != SHT_NOBITS) &&
                ((*it)->getAddr() - last->getAddr()) != ((*it)->getOffset() - last->getOffset())) {
                    throw std::runtime_error("Segments inconsistency");
            }
        }
    }
    // fixup ehdr before writing
    if (ehdr->e_phnum != segments.size()) {
        ehdr->e_phnum = segments.size();
        phdr_section->getShdr().sh_size = segments.size() * Elf_Phdr::size(ehdr->e_ident[EI_CLASS]);
        phdr_section->getNext()->markDirty();
    }
    // fixup shdr before writing
    if (ehdr->e_shnum != shdr_section->getSize() / shdr_section->getEntSize())
        shdr_section->getShdr().sh_size = ehdr->e_shnum * Elf_Shdr::size(ehdr->e_ident[EI_CLASS]);
    ehdr->e_shoff = shdr_section->getOffset();
    ehdr->e_entry = eh_entry.getValue();
    ehdr->e_shstrndx = eh_shstrndx->getIndex();

    // Check sections consistency
    unsigned int minOffset = 0;
    for (ElfSection *section = ehdr; section != nullptr; section = section->getNext()) {
        unsigned int offset = section->getOffset();
        if (offset < minOffset) {
           throw std::runtime_error("Sections overlap");
        }
        if (section->getType() != SHT_NOBITS) {
           minOffset = offset + section->getSize();
        }
    }
}

void Elf::write(std::ofstream &file)
{
    normalize();
    for (ElfSection *section = ehdr;
         section != nullptr; section = section->getNext()) {
        file.seekp(section->getOffset());
        if (section == phdr_section) {
            for (std::vector<ElfSegment *>::iterator seg = segments.begin(); seg != segments.end(); seg++) {
                Elf_Phdr phdr;
                phdr.p_type = (*seg)->getType();
                phdr.p_flags = (*seg)->getFlags();
                phdr.p_offset = (*seg)->getOffset();
                phdr.p_vaddr = (*seg)->getAddr();
                phdr.p_paddr = phdr.p_vaddr + (*seg)->getVPDiff();
                phdr.p_filesz = (*seg)->getFileSize();
                phdr.p_memsz = (*seg)->getMemSize();
                phdr.p_align = (*seg)->getAlign();
                phdr.serialize(file, ehdr->e_ident[EI_CLASS], ehdr->e_ident[EI_DATA]);
            }
        } else if (section == shdr_section) {
            null_section.serialize(file, ehdr->e_ident[EI_CLASS], ehdr->e_ident[EI_DATA]);
            for (ElfSection *sec = ehdr; sec!= nullptr; sec = sec->getNext()) {
                if (sec->getType() != SHT_NULL)
                    sec->getShdr().serialize(file, ehdr->e_ident[EI_CLASS], ehdr->e_ident[EI_DATA]);
            }
        } else
           section->serialize(file, ehdr->e_ident[EI_CLASS], ehdr->e_ident[EI_DATA]);
    }
}

ElfSection::ElfSection(Elf_Shdr &s, std::ifstream *file, Elf *parent)
: shdr(s),
  link(shdr.sh_link == SHN_UNDEF ? nullptr : parent->getSection(shdr.sh_link)),
  next(nullptr), previous(nullptr), index(-1)
{
    if ((file == nullptr) || (shdr.sh_type == SHT_NULL) || (shdr.sh_type == SHT_NOBITS))
        data = nullptr;
    else {
        data = new char[shdr.sh_size];
        int pos = file->tellg();
        file->seekg(shdr.sh_offset);
        file->read(data, shdr.sh_size);
        file->seekg(pos);
    }
    if (shdr.sh_name == 0)
        name = nullptr;
    else {
        ElfStrtab_Section *strtab = (ElfStrtab_Section *) parent->getSection(-1);
        // Special case (see elfgeneric.cpp): if strtab is nullptr, the
        // section being created is the strtab.
        if (strtab == nullptr)
            name = &data[shdr.sh_name];
        else
            name = strtab->getStr(shdr.sh_name);
    }
    // Only SHT_REL/SHT_RELA sections use sh_info to store a section
    // number.
    if ((shdr.sh_type == SHT_REL) || (shdr.sh_type == SHT_RELA))
        info.section = shdr.sh_info ? parent->getSection(shdr.sh_info) : nullptr;
    else
        info.index = shdr.sh_info;
}

unsigned int ElfSection::getAddr()
{
    if (shdr.sh_addr != (Elf32_Word)-1)
        return shdr.sh_addr;

    // It should be safe to adjust sh_addr for all allocated sections that
    // are neither SHT_NOBITS nor SHT_PROGBITS
    if ((previous != nullptr) && isRelocatable()) {
        unsigned int addr = previous->getAddr();
        if (previous->getType() != SHT_NOBITS)
            addr += previous->getSize();

        if (addr & (getAddrAlign() - 1))
            addr = (addr | (getAddrAlign() - 1)) + 1;

        return (shdr.sh_addr = addr);
    }
    return shdr.sh_addr;
}

unsigned int ElfSection::getOffset()
{
    if (shdr.sh_offset != (Elf32_Word)-1)
        return shdr.sh_offset;

    if (previous == nullptr)
        return (shdr.sh_offset = 0);

    unsigned int offset = previous->getOffset();

    ElfSegment *ptload = getSegmentByType(PT_LOAD);
    ElfSegment *prev_ptload = previous->getSegmentByType(PT_LOAD);

    if (ptload && (ptload == prev_ptload)) {
        offset += getAddr() - previous->getAddr();
        return (shdr.sh_offset = offset);
    }

    if (previous->getType() != SHT_NOBITS)
        offset += previous->getSize();

    Elf32_Word align = 0x1000;
    for (std::vector<ElfSegment *>::iterator seg = segments.begin(); seg != segments.end(); seg++)
        align = std::max(align, (*seg)->getAlign());

    Elf32_Word mask = align - 1;
    // SHF_TLS is used for .tbss which is some kind of special case.
    if (((getType() != SHT_NOBITS) || (getFlags() & SHF_TLS)) && (getFlags() & SHF_ALLOC)) {
        if ((getAddr() & mask) < (offset & mask))
            offset = (offset | mask) + (getAddr() & mask) + 1;
        else
            offset = (offset & ~mask) + (getAddr() & mask);
    }
    if ((getType() != SHT_NOBITS) && (offset & (getAddrAlign() - 1)))
        offset = (offset | (getAddrAlign() - 1)) + 1;

    // Two subsequent sections can't be mapped in the same page in memory
    // if they aren't in the same 4K block on disk.
    if ((getType() != SHT_NOBITS) && getAddr()) {
        if (((offset >> 12) != (previous->getOffset() >> 12)) &&
            ((getAddr() >> 12) == (previous->getAddr() >> 12)))
            throw std::runtime_error("Moving section would require overlapping segments");
    }

    return (shdr.sh_offset = offset);
}

int ElfSection::getIndex()
{
    if (index != -1)
        return index;
    if (getType() == SHT_NULL)
        return (index = 0);
    ElfSection *reference;
    for (reference = previous; (reference != nullptr) && (reference->getType() == SHT_NULL); reference = reference->getPrevious());
    if (reference == nullptr)
        return (index = 1);
    return (index = reference->getIndex() + 1);
}

Elf_Shdr &ElfSection::getShdr()
{
    getOffset();
    if (shdr.sh_link == (Elf32_Word)-1)
        shdr.sh_link = getLink() ? getLink()->getIndex() : 0;
    if (shdr.sh_info == (Elf32_Word)-1)
        shdr.sh_info = ((getType() == SHT_REL) || (getType() == SHT_RELA)) ?
                       (getInfo().section ? getInfo().section->getIndex() : 0) :
                       getInfo().index;

    return shdr;
}

ElfSegment::ElfSegment(Elf_Phdr *phdr)
: type(phdr->p_type), v_p_diff(phdr->p_paddr - phdr->p_vaddr),
  flags(phdr->p_flags), align(phdr->p_align), vaddr(phdr->p_vaddr),
  filesz(phdr->p_filesz), memsz(phdr->p_memsz) {}

void ElfSegment::addSection(ElfSection *section)
{
    // Make sure all sections in PT_GNU_RELRO won't be moved by elfhack
    assert(!((type == PT_GNU_RELRO) && (section->isRelocatable())));

    //TODO: Check overlapping sections
    std::list<ElfSection *>::iterator i;
    for (i = sections.begin(); i != sections.end(); ++i)
        if ((*i)->getAddr() > section->getAddr())
            break;
    sections.insert(i, section);
    section->addToSegment(this);
}

void ElfSegment::removeSection(ElfSection *section)
{
    sections.remove(section);
    section->removeFromSegment(this);
}

unsigned int ElfSegment::getFileSize()
{
    if (type == PT_GNU_RELRO || isElfHackFillerSegment())
        return filesz;

    if (sections.empty())
        return 0;
    // Search the last section that is not SHT_NOBITS
    std::list<ElfSection *>::reverse_iterator i;
    for (i = sections.rbegin(); (i != sections.rend()) && ((*i)->getType() == SHT_NOBITS); ++i);
    // All sections are SHT_NOBITS
    if (i == sections.rend())
        return 0;

    unsigned int end = (*i)->getAddr() + (*i)->getSize();

    return end - sections.front()->getAddr();
}

unsigned int ElfSegment::getMemSize()
{
    if (type == PT_GNU_RELRO || isElfHackFillerSegment())
        return memsz;

    if (sections.empty())
        return 0;

    unsigned int end = sections.back()->getAddr() + sections.back()->getSize();

    return end - sections.front()->getAddr();
}

unsigned int ElfSegment::getOffset()
{
    if ((type == PT_GNU_RELRO) && !sections.empty() &&
        (sections.front()->getAddr() != vaddr))
        throw std::runtime_error("PT_GNU_RELRO segment doesn't start on a section start");

    // Neither bionic nor glibc linkers seem to like when the offset of that segment is 0
    if (isElfHackFillerSegment())
        return vaddr;

    return sections.empty() ? 0 : sections.front()->getOffset();
}

unsigned int ElfSegment::getAddr()
{
    if ((type == PT_GNU_RELRO) && !sections.empty() &&
        (sections.front()->getAddr() != vaddr))
        throw std::runtime_error("PT_GNU_RELRO segment doesn't start on a section start");

    if (isElfHackFillerSegment())
        return vaddr;

    return sections.empty() ? 0 : sections.front()->getAddr();
}

void ElfSegment::clear()
{
    for (std::list<ElfSection *>::iterator i = sections.begin(); i != sections.end(); ++i)
        (*i)->removeFromSegment(this);
    sections.clear();
}

ElfValue *ElfDynamic_Section::getValueForType(unsigned int tag)
{
    for (unsigned int i = 0; i < shdr.sh_size / shdr.sh_entsize; i++)
        if (dyns[i].tag == tag)
            return dyns[i].value;

    return nullptr;
}

ElfSection *ElfDynamic_Section::getSectionForType(unsigned int tag)
{
    ElfValue *value = getValueForType(tag);
    return value ? value->getSection() : nullptr;
}

bool ElfDynamic_Section::setValueForType(unsigned int tag, ElfValue *val)
{
    unsigned int i;
    unsigned int shnum = shdr.sh_size / shdr.sh_entsize;
    for (i = 0; (i < shnum) && (dyns[i].tag != DT_NULL); i++)
        if (dyns[i].tag == tag) {
            delete dyns[i].value;
            dyns[i].value = val;
            return true;
        }
    // If we get here, this means we didn't match for the given tag
    // Most of the time, there are a few DT_NULL entries, that we can
    // use to add our value, but if we are on the last entry, we can't.
    if (i >= shnum - 1)
        return false;

    dyns[i].tag = tag;
    dyns[i].value = val;
    return true;
}

ElfDynamic_Section::ElfDynamic_Section(Elf_Shdr &s, std::ifstream *file, Elf *parent)
: ElfSection(s, file, parent)
{
    int pos = file->tellg();
    dyns.resize(s.sh_size / s.sh_entsize);
    file->seekg(shdr.sh_offset);
    // Here we assume tags refer to only one section (e.g. DT_RELSZ accounts
    // for .rel.dyn size)
    for (unsigned int i = 0; i < s.sh_size / s.sh_entsize; i++) {
        Elf_Dyn dyn(*file, parent->getClass(), parent->getData());
        dyns[i].tag = dyn.d_tag;
        switch (dyn.d_tag) {
        case DT_NULL:
        case DT_SYMBOLIC:
        case DT_TEXTREL:
        case DT_BIND_NOW:
            dyns[i].value = new ElfValue();
            break;
        case DT_NEEDED:
        case DT_SONAME:
        case DT_RPATH:
        case DT_PLTREL:
        case DT_RUNPATH:
        case DT_FLAGS:
        case DT_RELACOUNT:
        case DT_RELCOUNT:
        case DT_VERDEFNUM:
        case DT_VERNEEDNUM:
            dyns[i].value = new ElfPlainValue(dyn.d_un.d_val);
            break;
        case DT_PLTGOT:
        case DT_HASH:
        case DT_STRTAB:
        case DT_SYMTAB:
        case DT_RELA:
        case DT_INIT:
        case DT_FINI:
        case DT_REL:
        case DT_JMPREL:
        case DT_INIT_ARRAY:
        case DT_FINI_ARRAY:
        case DT_GNU_HASH:
        case DT_VERSYM:
        case DT_VERNEED:
        case DT_VERDEF:
            dyns[i].value = new ElfLocation(dyn.d_un.d_ptr, parent);
            break;
        default:
            dyns[i].value = nullptr;
        }
    }
    // Another loop to get the section sizes
    for (unsigned int i = 0; i < s.sh_size / s.sh_entsize; i++)
        switch (dyns[i].tag) {
        case DT_PLTRELSZ:
            dyns[i].value = new ElfSize(getSectionForType(DT_JMPREL));
            break;
        case DT_RELASZ:
            dyns[i].value = new ElfSize(getSectionForType(DT_RELA));
            break;
        case DT_STRSZ:
            dyns[i].value = new ElfSize(getSectionForType(DT_STRTAB));
            break;
        case DT_RELSZ:
            dyns[i].value = new ElfSize(getSectionForType(DT_REL));
            break;
        case DT_INIT_ARRAYSZ:
            dyns[i].value = new ElfSize(getSectionForType(DT_INIT_ARRAY));
            break;
        case DT_FINI_ARRAYSZ:
            dyns[i].value = new ElfSize(getSectionForType(DT_FINI_ARRAY));
            break;
        case DT_RELAENT:
            dyns[i].value = new ElfEntSize(getSectionForType(DT_RELA));
            break;
        case DT_SYMENT:
            dyns[i].value = new ElfEntSize(getSectionForType(DT_SYMTAB));
            break;
        case DT_RELENT:
            dyns[i].value = new ElfEntSize(getSectionForType(DT_REL));
            break;
        }

    file->seekg(pos);
}

ElfDynamic_Section::~ElfDynamic_Section()
{
    for (unsigned int i = 0; i < shdr.sh_size / shdr.sh_entsize; i++)
        delete dyns[i].value;
}

void ElfDynamic_Section::serialize(std::ofstream &file, char ei_class, char ei_data)
{
    for (unsigned int i = 0; i < shdr.sh_size / shdr.sh_entsize; i++) {
        Elf_Dyn dyn;
        dyn.d_tag = dyns[i].tag;
        dyn.d_un.d_val = (dyns[i].value != nullptr) ? dyns[i].value->getValue() : 0;
        dyn.serialize(file, ei_class, ei_data);
    }
}

ElfSymtab_Section::ElfSymtab_Section(Elf_Shdr &s, std::ifstream *file, Elf *parent)
: ElfSection(s, file, parent)
{
    int pos = file->tellg();
    syms.resize(s.sh_size / s.sh_entsize);
    ElfStrtab_Section *strtab = (ElfStrtab_Section *)getLink();
    file->seekg(shdr.sh_offset);
    for (unsigned int i = 0; i < shdr.sh_size / shdr.sh_entsize; i++) {
        Elf_Sym sym(*file, parent->getClass(), parent->getData());
        syms[i].name = strtab->getStr(sym.st_name);
        syms[i].info = sym.st_info;
        syms[i].other = sym.st_other;
        ElfSection *section = (sym.st_shndx == SHN_ABS) ? nullptr : parent->getSection(sym.st_shndx);
        new (&syms[i].value) ElfLocation(section, sym.st_value, ElfLocation::ABSOLUTE);
        syms[i].size = sym.st_size;
        syms[i].defined = (sym.st_shndx != SHN_UNDEF);
    }
    file->seekg(pos);
}

void
ElfSymtab_Section::serialize(std::ofstream &file, char ei_class, char ei_data)
{
    ElfStrtab_Section *strtab = (ElfStrtab_Section *)getLink();
    for (unsigned int i = 0; i < shdr.sh_size / shdr.sh_entsize; i++) {
        Elf_Sym sym;
        sym.st_name = strtab->getStrIndex(syms[i].name);
        sym.st_info = syms[i].info;
        sym.st_other = syms[i].other;
        sym.st_value = syms[i].value.getValue();
        ElfSection *section = syms[i].value.getSection();
        if (syms[i].defined)
            sym.st_shndx = section ? section->getIndex() : SHN_ABS;
        else
            sym.st_shndx = SHN_UNDEF;
        sym.st_size = syms[i].size;
        sym.serialize(file, ei_class, ei_data);
    }
}

Elf_SymValue *
ElfSymtab_Section::lookup(const char *name, unsigned int type_filter)
{
    for (std::vector<Elf_SymValue>::iterator sym = syms.begin();
         sym != syms.end(); sym++) {
        if ((type_filter & (1 << ELF32_ST_TYPE(sym->info))) &&
            (strcmp(sym->name, name) == 0)) {
            return &*sym;
        }
    }
    return nullptr;
}

const char *
ElfStrtab_Section::getStr(unsigned int index)
{
    for (std::vector<table_storage>::iterator t = table.begin();
         t != table.end(); t++) {
        if (index < t->used)
            return t->buf + index;
        index -= t->used;
    }
    assert(1 == 0);
    return nullptr;
}

const char *
ElfStrtab_Section::getStr(const char *string)
{
    if (string == nullptr)
        return nullptr;

    // If the given string is within the section, return it
    for (std::vector<table_storage>::iterator t = table.begin();
         t != table.end(); t++)
        if ((string >= t->buf) && (string < t->buf + t->used))
            return string;

    // TODO: should scan in the section to find an existing string

    // If not, we need to allocate the string in the section
    size_t len = strlen(string) + 1;

    if (table.back().size - table.back().used < len)
        table.resize(table.size() + 1);

    char *alloc_str = table.back().buf + table.back().used;
    memcpy(alloc_str, string, len);
    table.back().used += len;

    shdr.sh_size += len;
    markDirty();

    return alloc_str;
}

unsigned int
ElfStrtab_Section::getStrIndex(const char *string)
{
    if (string == nullptr)
        return 0;

    unsigned int index = 0;
    string = getStr(string);
    for (std::vector<table_storage>::iterator t = table.begin();
         t != table.end(); t++) {
        if ((string >= t->buf) && (string < t->buf + t->used))
            return index + (string - t->buf);
        index += t->used;
    }

    assert(1 == 0);
    return 0;
}

void
ElfStrtab_Section::serialize(std::ofstream &file, char ei_class, char ei_data)
{
    file.seekp(getOffset());
    for (std::vector<table_storage>::iterator t = table.begin();
         t != table.end(); t++)
        file.write(t->buf, t->used);
}
