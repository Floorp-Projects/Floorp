/* elf_gc_dynst
 *
 * This is a program that removes unreferenced strings from the .dynstr
 * section in ELF shared objects. It also shrinks the .dynstr section and
 * relocates all symbols after it.
 *
 * This program was written and copyrighted by:
 *   Alexander Larsson <alla@lysator.liu.se>
 *
 *
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <elf.h>
#include <glib.h>
#include <string.h>


Elf32_Ehdr *elf_header = NULL;
#define FILE_OFFSET(offset) ((unsigned char *)(elf_header) + (offset))

struct dynamic_symbol {
  Elf32_Word old_index;
  Elf32_Word new_index;
  char *string;
};

GHashTable *used_dynamic_symbols = NULL;
/* Data is dynamic_symbols, hashes on old_index */
Elf32_Word hole_index;
Elf32_Word hole_end;
Elf32_Word hole_len;

Elf32_Addr hole_addr_start;
Elf32_Addr hole_addr_remap_start;
Elf32_Addr hole_addr_remap_end;

int need_byteswap;

unsigned char machine_type;

Elf32_Word
read_word(Elf32_Word w)
{
  if (need_byteswap) 
    w = GUINT32_SWAP_LE_BE(w);
  return w;
}

Elf32_Sword
read_sword(Elf32_Sword w)
{
  if (need_byteswap) 
    w = (Elf32_Sword)GUINT32_SWAP_LE_BE((guint32)w);
  return w;
}

void 
write_word(Elf32_Word *ptr, Elf32_Word w)
{
  if (need_byteswap) 
    w = GUINT32_SWAP_LE_BE(w);
  *ptr = w;
}

Elf32_Half
read_half(Elf32_Half h)
{
  if (need_byteswap) 
    h = GUINT16_SWAP_LE_BE(h);
  return h;
}

void 
write_half(Elf32_Half *ptr, Elf32_Half h)
{
  if (need_byteswap) 
    h = GUINT16_SWAP_LE_BE(h);
  *ptr = h;
}

void
setup_byteswapping(unsigned char ei_data)
{
  need_byteswap = 0;
#if G_BYTE_ORDER == G_BIG_ENDIAN
  if (ei_data == ELFDATA2LSB)
    need_byteswap = 1;
#endif
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  if (ei_data == ELFDATA2MSB)
    need_byteswap = 1;
#endif
}


Elf32_Shdr *
elf_find_section_num(int section_index)
{
  Elf32_Shdr *section;
  Elf32_Word sectionsize;

  section = (Elf32_Shdr *)FILE_OFFSET(read_word(elf_header->e_shoff));
  sectionsize = read_half(elf_header->e_shentsize);

  section = (Elf32_Shdr *)((char *)section + sectionsize*section_index);

  return section;
}

Elf32_Shdr *
elf_find_section_named(char *name)
{
  Elf32_Shdr *section;
  Elf32_Shdr *strtab_section;
  Elf32_Word sectionsize;
  int numsections;
  char *strtab;
  int i = 0;

  section = (Elf32_Shdr *)FILE_OFFSET(read_word(elf_header->e_shoff));

  strtab_section = elf_find_section_num(read_half(elf_header->e_shstrndx));
  
  strtab = (char *)FILE_OFFSET(read_word(strtab_section->sh_offset));
  
  sectionsize = read_half(elf_header->e_shentsize);
  numsections = read_half(elf_header->e_shnum);

  for (i=0;i<numsections;i++) {
    if (strcmp(&strtab[read_word(section->sh_name)], name) == 0) {
      return section;
    }
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }
  return NULL;
}


Elf32_Shdr *
elf_find_section(Elf32_Word sh_type)
{
  Elf32_Shdr *section;
  Elf32_Word sectionsize;
  int numsections;
  int i = 0;

  section = (Elf32_Shdr *)FILE_OFFSET(read_word(elf_header->e_shoff));
  sectionsize = read_half(elf_header->e_shentsize);
  numsections = read_half(elf_header->e_shnum);

  for (i=0;i<numsections;i++) {
    if (read_word(section->sh_type) == sh_type) {
      return section;
    }
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }
  return NULL;
}

Elf32_Shdr *
elf_find_next_higher_section(Elf32_Word offset)
{
  Elf32_Shdr *section;
  Elf32_Shdr *higher;
  Elf32_Word sectionsize;
  int numsections;
  int i = 0;

  section = (Elf32_Shdr *)FILE_OFFSET(read_word(elf_header->e_shoff));
  sectionsize = read_half(elf_header->e_shentsize);
  numsections = read_half(elf_header->e_shnum);

  higher = NULL;

  for (i=0;i<numsections;i++) {
    if (read_word(section->sh_offset) >= offset) {
      if (higher == NULL) {
	higher = section;
      } else if (read_word(section->sh_offset) < read_word(higher->sh_offset)) {
	higher = section;
      }
    }
    
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }
  
  return higher;
}

Elf32_Word
vma_to_offset(Elf32_Addr addr)
{
  Elf32_Shdr *section;
  Elf32_Shdr *higher;
  Elf32_Word sectionsize;
  int numsections;
  int i = 0;

  section = (Elf32_Shdr *)FILE_OFFSET(read_word(elf_header->e_shoff));
  sectionsize = read_half(elf_header->e_shentsize);
  numsections = read_half(elf_header->e_shnum);

  higher = NULL;

  for (i=0;i<numsections;i++) {
    if ( (addr >= read_word(section->sh_addr)) &&
	 (addr < read_word(section->sh_addr) + read_word(section->sh_size)) ) {
      return read_word(section->sh_offset) + (addr - read_word(section->sh_addr));
    }
    
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }

  fprintf(stderr, "Warning, unable to convert address %d (0x%x) to file offset\n",
	 addr, addr);
  return 0;
}


void
find_segment_addr_min_max(Elf32_Word file_offset,
			  Elf32_Addr *start, Elf32_Addr *end)
{
  Elf32_Phdr *segment;
  Elf32_Word segmentsize;
  int numsegments;
  int i = 0;

  segment = (Elf32_Phdr *)FILE_OFFSET(read_word(elf_header->e_phoff));
  segmentsize = read_half(elf_header->e_phentsize);
  numsegments = read_half(elf_header->e_phnum);

  for (i=0;i<numsegments;i++) {
    if ((file_offset >= read_word(segment->p_offset)) &&
	(file_offset < read_word(segment->p_offset) + read_word(segment->p_filesz)))  {
      *start = read_word(segment->p_vaddr);
      *end = read_word(segment->p_vaddr) + read_word(segment->p_memsz);
      return;
    }

    segment = (Elf32_Phdr *)((char *)segment + segmentsize);
  }
  fprintf(stderr, "Error: Couldn't find segment in find_segment_addr_min_max()\n");
}

void *
dynamic_find_tag(Elf32_Shdr *dynamic, Elf32_Sword d_tag)
{
  int i;
  Elf32_Dyn *element;

  element = (Elf32_Dyn *)FILE_OFFSET(read_word(dynamic->sh_offset));
  for (i=0; read_sword(element[i].d_tag) != DT_NULL; i++) {
    if (read_sword(element[i].d_tag) == d_tag) {
      return FILE_OFFSET(read_word(element[i].d_un.d_ptr));
    }
  }
  
  return NULL;
}

Elf32_Word
fixup_offset(Elf32_Word offset)
{
  if (offset >= hole_index) {
    return offset - hole_len;
  }
  return offset;
}

Elf32_Word
fixup_size(Elf32_Word offset, Elf32_Word size)
{
  /* Note: Doesn't handle the cases where the hole and the size intersect
     partially. */
  
  if ( (hole_index >= offset) &&
       (hole_index < offset + size)){
    return size - hole_len;
  }
  
  return size;
}

Elf32_Addr
fixup_addr(Elf32_Addr addr)
{
  if (addr == 0)
    return 0;

  /*
  if ( (addr < hole_addr_remap_start) ||
       (addr >= hole_addr_remap_end))
    return addr;
  */
  
  if (addr >= hole_addr_start) {
    return addr - hole_len;
  }
  return addr;
}

Elf32_Word
fixup_addr_size(Elf32_Addr addr, Elf32_Word size)
{
  /* Note: Doesn't handle the cases where the hole and the size intersect
     partially. */
  /*
  if ( (addr < hole_addr_remap_start) ||
       (addr >= hole_addr_remap_end))
    return size;
  */
  if ( (hole_addr_start >= addr) &&
       (hole_addr_start < addr + size)){
    return size - hole_len;
  }
  
  return size;
}

void
possibly_add_string(int name_idx, const char *name)
{
  struct dynamic_symbol *dynamic_symbol;
  if (name_idx != 0) {
    dynamic_symbol = g_hash_table_lookup(used_dynamic_symbols, (gpointer) name_idx);
    
    if (dynamic_symbol == NULL) {
      
      dynamic_symbol = g_new(struct dynamic_symbol, 1);
      
      dynamic_symbol->old_index = name_idx;
      dynamic_symbol->new_index = 0;
      dynamic_symbol->string = g_strdup(name);
      
      g_hash_table_insert(used_dynamic_symbols, (gpointer)name_idx, dynamic_symbol);
      /*printf("added dynamic string: %s (%d)\n", dynamic_symbol->string, name_idx);*/
    }
  }
}

Elf32_Word
fixup_string(Elf32_Word old_idx)
{
  struct dynamic_symbol *dynamic_symbol;

  if (old_idx == 0)
    return 0;
  
  dynamic_symbol = g_hash_table_lookup(used_dynamic_symbols, (gpointer) old_idx);

  if (dynamic_symbol == NULL) {
    fprintf(stderr, "AAAAAAAAAAAARGH!? Unknown string found in fixup (index: %d)!\n", old_idx);
    return 0;
  }
  
  return dynamic_symbol->new_index;
}



void
add_strings_from_dynsym(Elf32_Shdr *dynsym, char *strtab)
{
  Elf32_Sym *symbol;
  Elf32_Sym *symbol_end;
  Elf32_Word entry_size;
  

  symbol = (Elf32_Sym *)FILE_OFFSET(read_word(dynsym->sh_offset));
  symbol_end = (Elf32_Sym *)FILE_OFFSET(read_word(dynsym->sh_offset) + read_word(dynsym->sh_size));
  entry_size = read_word(dynsym->sh_entsize);

  while (symbol < symbol_end) {
    int name_idx;
    struct dynamic_symbol *dynamic_symbol;

    name_idx = read_word(symbol->st_name);
    possibly_add_string(name_idx, &strtab[name_idx]);

    
    symbol = (Elf32_Sym *)((char *)symbol + entry_size);
  }
}


void
fixup_strings_in_dynsym(Elf32_Shdr *dynsym)
{
  Elf32_Sym *symbol;
  Elf32_Sym *symbol_end;
  Elf32_Word entry_size;
  

  symbol = (Elf32_Sym *)FILE_OFFSET(read_word(dynsym->sh_offset));
  symbol_end = (Elf32_Sym *)FILE_OFFSET(read_word(dynsym->sh_offset) + read_word(dynsym->sh_size));
  entry_size = read_word(dynsym->sh_entsize);
  
  while (symbol < symbol_end) {
    struct dynamic_symbol *dynamic_symbol;

    write_word(&symbol->st_name,
	       fixup_string(read_word(symbol->st_name)));
			 
    symbol = (Elf32_Sym *)((char *)symbol + entry_size);
  }
}


void
add_strings_from_dynamic(Elf32_Shdr *dynamic, char *strtab)
{
  int i;
  int name_idx;
  Elf32_Dyn *element;
  Elf32_Word entry_size;

  entry_size = read_word(dynamic->sh_entsize);
  

  element = (Elf32_Dyn *)FILE_OFFSET(read_word(dynamic->sh_offset));
  while (read_sword(element->d_tag) != DT_NULL) {

    switch(read_sword(element->d_tag)) {
    case DT_NEEDED:
    case DT_SONAME:
    case DT_RPATH:
      name_idx = read_word(element->d_un.d_val);
      /*if (name_idx) printf("d_tag: %d\n", element->d_tag);*/
      possibly_add_string(name_idx, &strtab[name_idx]);
      break;
    default:
      ;
      /*printf("unhandled d_tag: %d (0x%x)\n", element->d_tag, element->d_tag);*/
    }

    element = (Elf32_Dyn *)((char *)element + entry_size);
  }
  
}

void
fixup_strings_in_dynamic(Elf32_Shdr *dynamic)
{
  int i;
  int name_idx;
  Elf32_Dyn *element;
  Elf32_Word entry_size;

  entry_size = read_word(dynamic->sh_entsize);

  element = (Elf32_Dyn *)FILE_OFFSET(read_word(dynamic->sh_offset));
  while (read_sword(element->d_tag) != DT_NULL) {

    switch(read_sword(element->d_tag)) {
    case DT_NEEDED:
    case DT_SONAME:
    case DT_RPATH:
      write_word(&element->d_un.d_val,
		 fixup_string(read_word(element->d_un.d_val)));
      break;
    default:
      ;
      /*printf("unhandled d_tag: %d (0x%x)\n", element->d_tag, element->d_tag);*/
    }

    element = (Elf32_Dyn *)((char *)element + entry_size);
  }
  
}


void
add_strings_from_ver_d(Elf32_Shdr *ver_d, char *strtab)
{
  Elf32_Verdaux *veraux;
  Elf32_Verdef *verdef;
  int num_aux;
  int name_idx;
  int i;
  int cont;

  verdef = (Elf32_Verdef *)FILE_OFFSET(read_word(ver_d->sh_offset));

  do {
    num_aux = read_half(verdef->vd_cnt);
    veraux = (Elf32_Verdaux *)((char *)verdef + read_word(verdef->vd_aux));
    for (i=0; i<num_aux; i++) {
      name_idx = read_word(veraux->vda_name);
      possibly_add_string(name_idx, &strtab[name_idx]);
      veraux = (Elf32_Verdaux *)((char *)veraux + read_word(veraux->vda_next));
    }

    cont = read_word(verdef->vd_next) != 0;
    verdef = (Elf32_Verdef *)((char *)verdef + read_word(verdef->vd_next));
  } while (cont);
  
}

void
fixup_strings_in_ver_d(Elf32_Shdr *ver_d)
{
  Elf32_Verdaux *veraux;
  Elf32_Verdef *verdef;
  int num_aux;
  int name_idx;
  int i;
  int cont;

  verdef = (Elf32_Verdef *)FILE_OFFSET(read_word(ver_d->sh_offset));

  do {
    num_aux = read_half(verdef->vd_cnt);
    veraux = (Elf32_Verdaux *)((char *)verdef + read_word(verdef->vd_aux));
    for (i=0; i<num_aux; i++) {
      write_word(&veraux->vda_name,
		 fixup_string(read_word(veraux->vda_name)));
      veraux = (Elf32_Verdaux *)((char *)veraux + read_word(veraux->vda_next));
    }

    cont = read_word(verdef->vd_next) != 0;
    verdef = (Elf32_Verdef *)((char *)verdef + read_word(verdef->vd_next));
  } while (cont);
  
}

void
add_strings_from_ver_r(Elf32_Shdr *ver_r, char *strtab)
{
  Elf32_Vernaux *veraux;
  Elf32_Verneed *verneed;
  int num_aux;
  int name_idx;
  int i;
  int cont;

  verneed = (Elf32_Verneed *)FILE_OFFSET(read_word(ver_r->sh_offset));

  do {
    name_idx = read_word(verneed->vn_file);
    possibly_add_string(name_idx, &strtab[name_idx]);
    num_aux = read_half(verneed->vn_cnt);
    veraux = (Elf32_Vernaux *)((char *)verneed + read_word(verneed->vn_aux));
    for (i=0; i<num_aux; i++) {
      name_idx = read_word(veraux->vna_name);
      possibly_add_string(name_idx, &strtab[name_idx]);
      veraux = (Elf32_Vernaux *)((char *)veraux + read_word(veraux->vna_next));
    }

    cont = read_word(verneed->vn_next) != 0;
    verneed = (Elf32_Verneed *)((char *)verneed + read_word(verneed->vn_next));
  } while (cont);
}

void
fixup_strings_in_ver_r(Elf32_Shdr *ver_r)
{
  Elf32_Vernaux *veraux;
  Elf32_Verneed *verneed;
  int num_aux;
  int name_idx;
  int i;
  int cont;

  verneed = (Elf32_Verneed *)FILE_OFFSET(read_word(ver_r->sh_offset));

  do {
    write_word(&verneed->vn_file,
	       fixup_string(read_word(verneed->vn_file)));
    num_aux = read_half(verneed->vn_cnt);
    veraux = (Elf32_Vernaux *)((char *)verneed + read_word(verneed->vn_aux));
    for (i=0; i<num_aux; i++) {
      write_word(&veraux->vna_name,
		 fixup_string(read_word(veraux->vna_name)));
      veraux = (Elf32_Vernaux *)((char *)veraux + read_word(veraux->vna_next));
    }

    cont = read_word(verneed->vn_next) != 0;
    verneed = (Elf32_Verneed *)((char *)verneed + read_word(verneed->vn_next));
  } while (cont);
}

gboolean sum_size(gpointer	key,
		  struct dynamic_symbol *sym,
		  int *size)
{
  *size += strlen(sym->string) + 1;
  return 1;
}

struct index_n_dynstr {
  int index;
  unsigned char *dynstr;
};

gboolean output_string(gpointer	key,
		       struct dynamic_symbol *sym,
		       struct index_n_dynstr *x)
{
  sym->new_index = x->index;
  memcpy(x->dynstr + x->index, sym->string, strlen(sym->string) + 1);
  x->index += strlen(sym->string) + 1;
  return 1;
}


unsigned char *
generate_new_dynstr(Elf32_Word *size_out)
{
  int size;
  unsigned char *new_dynstr;
  struct index_n_dynstr x;

  size = 1; /* first a zero */
  g_hash_table_foreach	(used_dynamic_symbols,
			 (GHFunc)sum_size,
			 &size);


  new_dynstr = g_malloc(size);

  new_dynstr[0] = 0;
  x.index = 1;
  x.dynstr = new_dynstr;
  g_hash_table_foreach	(used_dynamic_symbols,
			 (GHFunc)output_string,
			 &x);
  
  *size_out = size;
  return new_dynstr;
}

void
remap_sections(void)
{
  Elf32_Shdr *section;
  Elf32_Word sectionsize;
  int numsections;
  int i = 0;

  section = (Elf32_Shdr *)FILE_OFFSET(read_word(elf_header->e_shoff));
  sectionsize = read_half(elf_header->e_shentsize);
  numsections = read_half(elf_header->e_shnum);

  for (i=0;i<numsections;i++) {
    write_word(&section->sh_size,
	       fixup_size(read_word(section->sh_offset),
			  read_word(section->sh_size)));
    write_word(&section->sh_offset,
	       fixup_offset(read_word(section->sh_offset)));
    write_word(&section->sh_addr,
	       fixup_addr(read_word(section->sh_addr)));
    
    section = (Elf32_Shdr *)((char *)section + sectionsize);
  }
}


void
remap_segments(void)
{
  Elf32_Phdr *segment;
  Elf32_Word segmentsize;
  Elf32_Word p_align;
  int numsegments;
  int i = 0;

  segment = (Elf32_Phdr *)FILE_OFFSET(read_word(elf_header->e_phoff));
  segmentsize = read_half(elf_header->e_phentsize);
  numsegments = read_half(elf_header->e_phnum);

  for (i=0;i<numsegments;i++) {
    write_word(&segment->p_filesz,
	       fixup_size(read_word(segment->p_offset),
			  read_word(segment->p_filesz)));
    write_word(&segment->p_offset,
	       fixup_offset(read_word(segment->p_offset)));

    write_word(&segment->p_memsz,
	       fixup_addr_size(read_word(segment->p_vaddr),
			       read_word(segment->p_memsz)));
    write_word(&segment->p_vaddr,
	       fixup_addr(read_word(segment->p_vaddr)));
    write_word(&segment->p_paddr,
	       read_word(segment->p_vaddr));

    /* Consistancy checking: */
    p_align = read_word(segment->p_align);
    if (p_align > 1) {
      if ((read_word(segment->p_vaddr) - read_word(segment->p_offset))%p_align != 0) {
	fprintf(stderr, "Warning, creating non-aligned segment addr: %x offset: %x allign: %x\n",
		read_word(segment->p_vaddr), read_word(segment->p_offset), p_align);
      }
    }
    
    segment = (Elf32_Phdr *)((char *)segment + segmentsize);
  }
}

void
remap_elf_header(void)
{
  write_word(&elf_header->e_phoff,
	     fixup_offset(read_word(elf_header->e_phoff)));
  write_word(&elf_header->e_shoff,
	     fixup_offset(read_word(elf_header->e_shoff)));

  write_word(&elf_header->e_entry,
	     fixup_addr(read_word(elf_header->e_entry)));
}

void
remap_symtab(Elf32_Shdr *symtab)
{
  Elf32_Sym *symbol;
  Elf32_Sym *symbol_end;
  Elf32_Word entry_size;

  symbol = (Elf32_Sym *)FILE_OFFSET(read_word(symtab->sh_offset));
  symbol_end = (Elf32_Sym *)FILE_OFFSET(read_word(symtab->sh_offset) +
					read_word(symtab->sh_size));
  entry_size = read_word(symtab->sh_entsize);

  while (symbol < symbol_end) {
    write_word(&symbol->st_value,
	       fixup_addr(read_word(symbol->st_value)));
    symbol = (Elf32_Sym *)((char *)symbol + entry_size);
  }
}


/* Ugly global variables: */
Elf32_Addr got_data_start = 0;
Elf32_Addr got_data_end = 0;


void
remap_rel_section(Elf32_Rel *rel, Elf32_Word size, Elf32_Word entry_size)
{
  Elf32_Rel *rel_end;
  Elf32_Word offset;
  Elf32_Addr *addr;
  Elf32_Word type;

  rel_end = (Elf32_Rel *)((char *)rel + size);

  while (rel < rel_end) {
    type = ELF32_R_TYPE(read_word(rel->r_info)); 
    switch (machine_type) {
    case EM_386:
      if ((type == R_386_RELATIVE) || (type == R_386_JMP_SLOT)) {
	/* We need to relocate the data this is pointing to too. */
	offset = vma_to_offset(read_word(rel->r_offset));
	
	addr =  (Elf32_Addr *)FILE_OFFSET(offset);
	write_word(addr, 
		   fixup_addr(read_word(*addr)));
      }
      write_word(&rel->r_offset,
		 fixup_addr(read_word(rel->r_offset)));
      break;
    case EM_PPC:
      /* The PPC always uses RELA relocations */
      break;
    }

    
    rel = (Elf32_Rel *)((char *)rel + entry_size);
  }
}

void
remap_rela_section(Elf32_Rela *rela, Elf32_Word size, Elf32_Word entry_size)
{
  Elf32_Rela *rela_end;
  Elf32_Addr *addr;
  Elf32_Word offset;
  Elf32_Word type;
  Elf32_Word bitmask;

  rela_end = (Elf32_Rela *)((char *)rela + size);

  while (rela < rela_end) {
    type = ELF32_R_TYPE(read_word(rela->r_info));
    switch (machine_type) {
    case EM_386:
      if ((type == R_386_RELATIVE) || (type == R_386_JMP_SLOT)) {
	/* We need to relocate the data this is pointing to too. */
	offset = vma_to_offset(read_word(rela->r_offset));
	
	addr =  (Elf32_Addr *)FILE_OFFSET(offset);
	write_word(addr,
		   fixup_addr(read_word(*addr)));
      }
      write_word(&rela->r_offset,
		 fixup_addr(read_word(rela->r_offset)));
      break;
    case EM_PPC:
/* Some systems do not have PowerPC relocations defined */
#ifdef R_PPC_NONE
      switch (type) {
      case R_PPC_RELATIVE:
	write_word((Elf32_Word *)&rela->r_addend,
		   fixup_addr(read_word(rela->r_addend)));
	/* Fall through for 32bit offset fixup */
      case R_PPC_ADDR32:
      case R_PPC_GLOB_DAT:
      case R_PPC_JMP_SLOT:
	write_word(&rela->r_offset,
		   fixup_addr(read_word(rela->r_offset)));
	break;
      case R_PPC_NONE:
	break;
      default:
	fprintf(stderr, "Warning, unhandled PPC relocation type %d\n", type);
      }
#endif
      break;
    }
    
    rela = (Elf32_Rela *)((char *)rela + entry_size);
  }
}

void 
remap_i386_got(void)
{
  Elf32_Shdr *got_section;
  Elf32_Addr *got;
  Elf32_Addr *got_end;
  Elf32_Word entry_size;

  got_section = elf_find_section_named(".got");
  if (got_section == NULL) {
    fprintf(stderr, "Warning, no .got section\n");
    return;
  }

  got_data_start = read_word(got_section->sh_offset);
  got_data_end = got_data_start + read_word(got_section->sh_size);
  
  got = (Elf32_Addr *)FILE_OFFSET(got_data_start);
  got_end = (Elf32_Addr *)FILE_OFFSET(got_data_end);
  entry_size = read_word(got_section->sh_entsize);

  write_word(got,
	     fixup_addr(read_word(*got))); /* Pointer to .dynamic */
}

void 
remap_ppc_got(void)
{
  Elf32_Shdr *got_section;
  Elf32_Addr *got;
  Elf32_Addr *got_end;
  Elf32_Word entry_size;

  got_section = elf_find_section_named(".got");
  if (got_section == NULL) {
    fprintf(stderr, "Warning, no .got section\n");
    return;
  }

  got_data_start = read_word(got_section->sh_offset);
  got_data_end = got_data_start + read_word(got_section->sh_size);
  
  got = (Elf32_Addr *)FILE_OFFSET(got_data_start);
  got_end = (Elf32_Addr *)FILE_OFFSET(got_data_end);
  entry_size = read_word(got_section->sh_entsize);

  /* Skip reserved part.
   * Note that this should really be found by finding the
   * _GLOBAL_OFFSET_TABLE symbol, as it could (according to
   * the spec) point to the middle of the got.
   */
  got = (Elf32_Addr *)((char *)got + entry_size); /* Skip blrl instruction */
  write_word(got,
	     fixup_addr(read_word(*got))); /* Pointer to .dynamic */
}


Elf32_Word
get_dynamic_val(Elf32_Shdr *dynamic, Elf32_Sword tag)
{
  Elf32_Dyn *element;
  Elf32_Word entry_size;

  entry_size = read_word(dynamic->sh_entsize);

  element = (Elf32_Dyn *)FILE_OFFSET(read_word(dynamic->sh_offset));
  while (read_sword(element->d_tag) != DT_NULL) {
    if (read_sword(element->d_tag) == tag) {
      return read_word(element->d_un.d_val);
    }
    element = (Elf32_Dyn *)((char *)element + entry_size);
  }
  return 0;
}

void
remap_dynamic(Elf32_Shdr *dynamic, Elf32_Word new_dynstr_size)
{
  Elf32_Dyn *element;
  Elf32_Word entry_size;
  Elf32_Word rel_size;
  Elf32_Word rel_entry_size;
  Elf32_Rel *rel;
  Elf32_Rela *rela;
  int jmprel_overlaps;
  Elf32_Word rel_start, rel_end, jmprel_start, jmprel_end;
    
  entry_size = read_word(dynamic->sh_entsize);

  /* Find out if REL/RELA and JMPREL overlaps: */
  if (get_dynamic_val(dynamic, DT_PLTREL) == DT_REL) {
    rel_start = get_dynamic_val(dynamic, DT_REL);
    rel_end = rel_start + get_dynamic_val(dynamic, DT_RELSZ);
  } else {
    rel_start = get_dynamic_val(dynamic, DT_RELA);
    rel_end = rel_start + get_dynamic_val(dynamic, DT_RELASZ);
  }
  jmprel_start = get_dynamic_val(dynamic, DT_JMPREL);
  
  jmprel_overlaps = 0;
  if ((jmprel_start >= rel_start) && (jmprel_start < rel_end))
    jmprel_overlaps = 1;
    
  element = (Elf32_Dyn *)FILE_OFFSET(read_word(dynamic->sh_offset));
  while (read_sword(element->d_tag) != DT_NULL) {
    switch(read_sword(element->d_tag)) {
    case DT_STRSZ:
      write_word(&element->d_un.d_val, new_dynstr_size);
      break;
    case DT_PLTGOT:
    case DT_HASH:
    case DT_STRTAB:
    case DT_INIT:
    case DT_FINI:
    case DT_VERDEF:
    case DT_VERNEED:
    case DT_VERSYM:
      write_word(&element->d_un.d_ptr,
		 fixup_addr(read_word(element->d_un.d_ptr)));
      break;
    case DT_JMPREL:
      rel_size = get_dynamic_val(dynamic, DT_PLTRELSZ);
      if (!jmprel_overlaps) {
	if (get_dynamic_val(dynamic, DT_PLTREL) == DT_REL) {
	  rel_entry_size = get_dynamic_val(dynamic, DT_RELENT);
	  rel = (Elf32_Rel *)FILE_OFFSET(vma_to_offset(read_word(element->d_un.d_ptr)));
	  remap_rel_section(rel, rel_size, rel_entry_size);
	} else {
	  rel_entry_size = get_dynamic_val(dynamic, DT_RELAENT);
	  rela = (Elf32_Rela *)FILE_OFFSET(vma_to_offset(read_word(element->d_un.d_ptr)));
	  remap_rela_section(rela, rel_size, rel_entry_size);
	}
      }
      write_word(&element->d_un.d_ptr,
		 fixup_addr(read_word(element->d_un.d_ptr)));
      break;
    case DT_REL:
      rel_size = get_dynamic_val(dynamic, DT_RELSZ);
      rel_entry_size = get_dynamic_val(dynamic, DT_RELENT);
      rel = (Elf32_Rel *)FILE_OFFSET(vma_to_offset(read_word(element->d_un.d_ptr)));
      remap_rel_section(rel, rel_size, rel_entry_size);

      write_word(&element->d_un.d_ptr,
		 fixup_addr(read_word(element->d_un.d_ptr)));
      break;
    case DT_RELA:
      rel_size = get_dynamic_val(dynamic, DT_RELASZ);
      rel_entry_size = get_dynamic_val(dynamic, DT_RELAENT);
      rela = (Elf32_Rela *)FILE_OFFSET(vma_to_offset(read_word(element->d_un.d_ptr)));
      remap_rela_section(rela, rel_size, rel_entry_size);

      write_word(&element->d_un.d_ptr,
		 fixup_addr(read_word(element->d_un.d_ptr)));
      break;
    default:
      /*printf("unhandled d_tag: %d (0x%x)\n", read_sword(element->d_tag), read_sword(element->d_tag));*/
      break;
    }

    element = (Elf32_Dyn *)((char *)element + entry_size);
  }
}

void
align_hole(Elf32_Word *start, Elf32_Word *end)
{
  Elf32_Word len;
  Elf32_Word align;
  Elf32_Shdr *section;
  Elf32_Word sectionsize;
  int numsections;
  int i = 0;
  int unaligned;
  
  len = *end - *start;
  align = 0;
    
  sectionsize = read_half(elf_header->e_shentsize);
  numsections = read_half(elf_header->e_shnum);
  do {
    section = (Elf32_Shdr *)FILE_OFFSET(read_word(elf_header->e_shoff));
    unaligned = 0;
    
    for (i=0;i<numsections;i++) {
      if ( (read_word(section->sh_addralign) > 1) &&
	   ( (read_word(section->sh_offset) - len + align)%read_word(section->sh_addralign) != 0) ) {
	unaligned = 1;
      }
      
      section = (Elf32_Shdr *)((char *)section + sectionsize);
    }

    if (unaligned) {
      align++;
    }
      
  } while (unaligned);

  *start += align;
}

int
main(int argc, char *argv[])
{
  int fd;
  unsigned char *mapping;
  Elf32_Word size;
  struct stat statbuf;
  Elf32_Shdr *dynamic;
  Elf32_Shdr *dynsym;
  Elf32_Shdr *symtab;
  Elf32_Shdr *dynstr;
  Elf32_Shdr *hash;
  Elf32_Shdr *higher_section;
  Elf32_Word dynstr_index;
  Elf32_Shdr *ver_r;
  Elf32_Shdr *ver_d;
  char *dynstr_data;
  unsigned char *new_dynstr;
  Elf32_Word old_dynstr_size;
  Elf32_Word new_dynstr_size;
  
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    return 1;
  }

  fd = open(argv[1], O_RDWR);
  if (fd == -1) {
    fprintf(stderr, "Cannot open file %s\n", argv[1]);
    return 1;
  }
  
  if (fstat(fd, &statbuf) == -1) {
    fprintf(stderr, "Cannot stat file %s\n", argv[1]);
    return 1;
  }
  
  size = statbuf.st_size;
    
  mapping = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (mapping == (unsigned char *)-1) {
    fprintf(stderr, "Cannot mmap file %s\n", argv[1]);
    return 1;
  }

  used_dynamic_symbols = g_hash_table_new(g_direct_hash, g_direct_equal);

  elf_header = (Elf32_Ehdr *)mapping;

  if (strncmp((void *)elf_header, ELFMAG, SELFMAG)!=0) {
    fprintf(stderr, "Not an ELF file\n");
    return 1;
  }

  if (elf_header->e_ident[EI_VERSION] != EV_CURRENT) {
    fprintf(stderr, "Wrong ELF file version\n");
    return 1;
  }

  if (elf_header->e_ident[EI_CLASS] != ELFCLASS32) {
    fprintf(stderr, "Only 32bit ELF files supported\n");
    return 1;
  }
  
  setup_byteswapping(elf_header->e_ident[EI_DATA]);

  machine_type = read_half(elf_header->e_machine);
  if ( (machine_type != EM_386) &&
       (machine_type != EM_PPC) ) {
    fprintf(stderr, "Unsupported architecture. Supported are: x86, ppc\n");
    return 1;
  }

  if (read_half(elf_header->e_type) != ET_DYN) {
    fprintf(stderr, "Not an ELF shared object\n");
    return 1;
  }
  
  dynamic = elf_find_section(SHT_DYNAMIC);
  dynsym = elf_find_section(SHT_DYNSYM);
  symtab = elf_find_section(SHT_SYMTAB);
  dynstr_index = read_word(dynsym->sh_link);
  dynstr = elf_find_section_num(dynstr_index);
  dynstr_data = (char *)FILE_OFFSET(read_word(dynstr->sh_offset));
  old_dynstr_size = read_word(dynstr->sh_size);
  ver_d = elf_find_section(SHT_GNU_verdef);
  ver_r = elf_find_section(SHT_GNU_verneed);
  hash = elf_find_section(SHT_HASH);

  /* Generate hash table with all used strings: */
  
  add_strings_from_dynsym(dynsym, dynstr_data);
  add_strings_from_dynamic(dynamic, dynstr_data);
  if (ver_d && (read_word(ver_d->sh_link) == dynstr_index))
    add_strings_from_ver_d(ver_d, dynstr_data);
  if (ver_r && (read_word(ver_r->sh_link) == dynstr_index))
    add_strings_from_ver_r(ver_r, dynstr_data);

  /* Generate new dynstr section from the used strings hashtable: */
  
  new_dynstr = generate_new_dynstr(&new_dynstr_size);
  /*
  printf("New dynstr size: %d\n", new_dynstr_size);
  printf("Old dynstr size: %d\n", old_dynstr_size);
  */
  
  if (new_dynstr_size >= old_dynstr_size) {
    return 0;
  }

  /* Fixup all references: */
  fixup_strings_in_dynsym(dynsym);
  fixup_strings_in_dynamic(dynamic);
  if (ver_d && (read_word(ver_d->sh_link) == dynstr_index))
    fixup_strings_in_ver_d(ver_d);
  if (ver_r && (read_word(ver_r->sh_link) == dynstr_index))
    fixup_strings_in_ver_r(ver_r);
  
  /* Copy over the new dynstr: */
  memcpy(dynstr_data, new_dynstr, new_dynstr_size);
  memset(dynstr_data + new_dynstr_size, ' ', old_dynstr_size-new_dynstr_size);

  /* Compact the dynstr section and the file: */

  /* 1. Set up the data for the fixup_offset() function: */
  hole_index = read_word(dynstr->sh_offset) + new_dynstr_size;
  higher_section = elf_find_next_higher_section(hole_index);
  hole_end = read_word(higher_section->sh_offset);

  align_hole(&hole_index, &hole_end);
  hole_len = hole_end - hole_index;

  hole_addr_start = hole_index; /* TODO: Fix this to something better */

  find_segment_addr_min_max(read_word(dynstr->sh_offset),
			    &hole_addr_remap_start, &hole_addr_remap_end);
  
  /*
  printf("Hole remap: 0x%lx - 0x%lx\n", hole_addr_remap_start, hole_addr_remap_end);

  printf("hole: %lu - %lu (%lu bytes)\n", hole_index, hole_end, hole_len);
  printf("hole: 0x%lx - 0x%lx (0x%lx bytes)\n", hole_index, hole_end, hole_len);
  */
  
  /* 2. Change all section and segment sizes and offsets: */
  remap_symtab(dynsym);
  if (symtab)
    remap_symtab(symtab);

  if (machine_type == EM_386)
    remap_i386_got();
  if (machine_type == EM_PPC)
    remap_ppc_got();
  
  remap_dynamic(dynamic, new_dynstr_size);
  remap_sections(); /* After this line the section headers are wrong */
  remap_segments();
  remap_elf_header();
    
  /* 3. Do the real compacting. */

  memmove(mapping + hole_index,
	  mapping + hole_index + hole_len,
	  size - (hole_index + hole_len));
  
  munmap(mapping, size);

  ftruncate(fd, size - hole_len);
  close(fd);

  return 0;
}



