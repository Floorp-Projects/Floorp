/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

#include "avmplus.h"

#include <time.h>
#include <stdio.h>
#include "coff.h"

// ignore deprecated function warnings from msc
#pragma warning(disable:4996)

	// Implementation of basic coff file format library
namespace avmplus
{
#if defined(AVMPLUS_IA32) && defined(DEBUGGER)
	namespace compiler
	{
		Coff::Coff()
		{
			initHeader();
		}

		Coff::~Coff()
		{
			for(int i=0; i<header.f_nscns; i++)
			{
				delete sections[i];
				sections[i] = 0;
			}

			for(unsigned int i=0; i<header.f_nsyms; i++)
			{
				delete symbols[i];
				symbols[i] = 0;
			}
		}

		void Coff::initHeader()
		{
			header.f_magic = I386MAGIC;
			header.f_nscns = 0;
			header.f_timdat = (uint32)(time(0));
			header.f_symptr = sizeof(CoffHeader_t);
			header.f_nsyms = 0;
			header.f_opthdr = 0;
			header.f_flags = F_RELFLG | F_EXEC | F_LNNO | F_AR32WR ;//| F_LSYMS;
		}

		void Coff::addSection(const char* ar, int numBytes, int startAddr, int /*endAddr*/)
		{
			int alignedSize = (numBytes+3) & ~3;

			SectionHeader_t* section = (SectionHeader_t*) new char[sizeof(SectionHeader_t)+alignedSize]; 
	//		assert(count < NUM_SECTIONS);
			sections[header.f_nscns++] = section;

			write((char*)&section->s_name, "text    ", 8);
			section->s_paddr = startAddr;
			section->s_vaddr = startAddr;
			section->s_size = alignedSize;
			section->s_scnptr = 0;
			section->s_relptr = 0;
			section->s_lnnoptr = 0;
			section->s_nreloc = 0;
			section->s_nlnno = 0;
			section->s_flags = STYP_TEXT;

			char* c = (char*)(section+1);
			write(c, ar, numBytes);

			// put out our empty alignment bytes
			for(int i=numBytes; i<alignedSize; i++)
				c[i] = 0;
		}

		void Coff::addSymbol(const char* s, int value)
		{
			int size = strlen(s);					 // the string size
			int allocSize = sizeof(int) + size + 1;  // string length + its content + null terminator

			SymbolEntry_t* symbol = (SymbolEntry_t*) new char[sizeof(SymbolEntry_t)+allocSize]; 
			symbols[header.f_nsyms++] = symbol;

			symbol->name.table.zero = 0;
			symbol->name.table.offset = 0;
			symbol->value = value;
			symbol->scnum = header.f_nscns; //header.f_nscns;
			symbol->type = 4;	// int
			symbol->sclass = 2;	// external 
			symbol->numaux = 0;  // no auxillary entries

			// now copy the string onto the end of the record
			char* c = (char*)(symbol+1);
			c = write(c, (char*)&size, sizeof(int));
			write(c, s, size+1);
		}

		char* Coff::write(char* dst, const char* src, int count)
		{
			for(int i=0; i<count; i++)
				dst[i] = src[i];
			return &dst[count];
		}

		/**
		* We dump out the coff file in the following order.  Where * denotes 1 or more entries
		*   CoffHeader_t
		*   *SectionHeader_t
		*   *<text>	
		*   <symboltable>
		*   <stringtable>
		*/
		void Coff::done()
		{
			FILE* f = fopen("avm.jit.o", "w");

			// Fill in the starting point for each of the section's data segments
			// which immeidately follow the section headers
			int pos = sizeof(CoffHeader_t) + (header.f_nscns * sizeof(SectionHeader_t));
			for(int i=0; i<header.f_nscns; i++)
			{
				sections[i]->s_scnptr = pos;
				pos += sections[i]->s_size;
			}

			// now pos points to our symbol table so update our main header
			header.f_symptr = pos;

			// point to after the symbol table.
			pos += header.f_nsyms * sizeof(SymbolEntry_t);

			// then into the string table (1st element is size of table)
			int strSize = pos; // note our starting point for the string table
			pos += 4; 

			// each symbol name lands in the sting table 
			for(unsigned int i=0; i<header.f_nsyms; i++)
			{
				symbols[i]->name.table.offset = pos - strSize;  // name's offset into string table
				int* size = (int*)(symbols[i]+1);     // the string is stored pascal style (i.e length leads content)
				pos += *size;						
				pos += 1;						 	 // null terminator
			}

			// now compute the string table size which include 4bytes of length
			strSize = (pos - strSize);

			// now we're ready to write...

			// write the main header
			int realPos = fwrite(&header, 1, sizeof(CoffHeader_t), f);
			
			// now write out all the headers of the sections
			for(int i=0; i<header.f_nscns; i++)
			{
				sections[i]->s_relptr = pos; // ptr to end of file => no entries
				sections[i]->s_lnnoptr = pos; // ptr to end of file => no entries
				realPos += fwrite(sections[i], 1, sizeof(SectionHeader_t), f);
			}

			// now the content of the sections
			for(int i=0; i<header.f_nscns; i++)
				realPos += fwrite(sections[i]+1, 1, sections[i]->s_size, f);

			// now the symbol table
			for(unsigned int i=0; i<header.f_nsyms; i++)
				realPos += fwrite(symbols[i], 1, sizeof(SymbolEntry_t), f);

			// now the string table	
			realPos += fwrite(&strSize, 1, 4, f);   // first its size little endian
			for(unsigned int i=0; i<header.f_nsyms; i++)
			{
				int* size = (int*)(symbols[i]+1);  // pascal style string after the entry
				realPos += fwrite(size+1, 1, *size+1, f);
			}

			// pad with some stuff 
	//		char* empty = "\0\0\0\0\0\0\0\0";
	//		realPos += fwrite(empty, 1, (realPos-7)&~realPos, f);

			//assert(realPos == pos);
			fclose(f);
		}
	}
#endif // DEBUGGER
}
