/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "avmshell.h"

#include <stdlib.h>

namespace avmshell
{
	BEGIN_NATIVE_MAP(FileClass)
		NATIVE_METHOD(avmplus_File_exists, FileClass::exists)
		NATIVE_METHOD(avmplus_File_read, FileClass::read)
		NATIVE_METHOD(avmplus_File_write, FileClass::write)
	END_NATIVE_MAP()
					  
	FileClass::FileClass(VTable *cvtable)
		: ClassClosure(cvtable)
    {
		createVanillaPrototype();
	}

	bool FileClass::exists(Stringp filename)
	{
		if (!filename) {
			toplevel()->throwArgumentError(kNullArgumentError, "filename");
		}
		UTF8String* filenameUTF8 = filename->toUTF8String();
		FILE *fp = fopen(filenameUTF8->c_str(), "r");
		if (fp != NULL) {
			fclose(fp);
			return true;
		}
		return false;
	}

	Stringp FileClass::read(Stringp filename)
	{
		Toplevel* toplevel = this->toplevel();
		AvmCore* core = this->core();

		if (!filename) {
			toplevel->throwArgumentError(kNullArgumentError, "filename");
		}
		UTF8String* filenameUTF8 = filename->toUTF8String();
		FILE *fp = fopen(filenameUTF8->c_str(), "r");
		if (fp == NULL) {
			toplevel->throwError(kFileOpenError, filename);
		}
		fseek(fp, 0L, SEEK_END);
		long len = ftell(fp);
		rewind(fp);

		unsigned char *c = new unsigned char[len+1];
		len = fread(c, 1, len, fp);
		c[len] = 0;
		
		fclose(fp);

		if (len >= 3)
		{
			// UTF8 BOM
			if ((c[0] == 0xef) && (c[1] == 0xbb) && (c[2] == 0xbf))
			{
				return core->newString(((char *)c) + 3, len - 3);
			}
			else if ((c[0] == 0xfe) && (c[1] == 0xff))
			{
				//UTF-16 big endian
				c += 2;
				len = (len - 2) >> 1;
				Stringp out = new (core->GetGC()) String(len);
				wchar *buffer = out->lockBuffer();
				for (long i = 0; i < len; i++)
				{
					buffer[i] = (c[0] << 8) + c[1];
					c += 2;
				}
				out->unlockBuffer();

				return out;
			}
			else if ((c[0] == 0xff) && (c[1] == 0xfe))
			{
				//UTF-16 little endian
				c += 2;
				len = (len - 2) >> 1;
				Stringp out = new (core->GetGC()) String(len);
				wchar *buffer = out->lockBuffer();
				for (long i = 0; i < len; i++)
				{
					buffer[i] = (c[1] << 8) + c[0];
					c += 2;
				}
				out->unlockBuffer();
				return out;
			}
		}

		Stringp out = core->newString((char *) c);
		delete [] c;
		
		return out;
	}

	void FileClass::write(Stringp filename,
						  Stringp data)
	{
		Toplevel* toplevel = this->toplevel();

		if (!filename) {
			toplevel->throwArgumentError(kNullArgumentError, "filename");
		}
		if (!data) {
			toplevel->throwArgumentError(kNullArgumentError, "data");
		}
		UTF8String* filenameUTF8 = filename->toUTF8String();
		FILE *fp = fopen(filenameUTF8->c_str(), "w");
		if (fp == NULL) {
			toplevel->throwError(kFileWriteError, filename);
		}
		UTF8String* dataUTF8 = data->toUTF8String();
		fwrite(dataUTF8->c_str(), dataUTF8->length(), 1, fp);
		fclose(fp);
	}
}
