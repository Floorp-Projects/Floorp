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
 * by the Initial Developer are Copyright (C)[ 1993-2001 ] Adobe Systems Incorporated. All Rights 
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



#ifndef GENERIC_ZLIB_H
#define GENERIC_ZLIB_H

namespace avmshell
{
	
//#include "platformutils.h"
#define PLAYERASSERT AvmAssert
#include "zlib.h"


inline bool PlatformZlibInflate(
							const U8* pbCompressed, 
							int nbCompressed,
							U8* pbDecompressedOut,
							int* pnbDecompressedOut
							)
{
	PLAYERASSERT(nbCompressed > 0);
	PLAYERASSERT(*pnbDecompressedOut >= nbCompressed);
	PLAYERASSERT(sizeof(uLongf) == sizeof(int)); // Ensure cast is safe

	int error = uncompress(
						pbDecompressedOut, 
						(uLongf*) pnbDecompressedOut, 
						pbCompressed, 
						nbCompressed
						);

	PLAYERASSERT(error == Z_OK);
	return (error == Z_OK);
}


/////////////////////////////////////////////////////////////////////////////


class PlatformZlibStream
{
public:
	inline PlatformZlibStream();
	inline ~PlatformZlibStream();

	//
	// Set compressed input stream
	//
	inline void SetNextIn(const U8* pbCompressed);	// set next input byte
	inline void SetAvailIn(int nb);						// set number of bytes available at next_in
	inline int AvailIn();								// get number of bytes available at next_in

	//
	// Try decompressing some bytes
	//
	inline bool Inflate();	// returns false at end of stream!
	inline int InflateWithStatus(); // returns the Z_* error code instead of true/false

	//
	// Get decompressed output stream
	//
	inline void SetNextOut(U8* pbDecompressed);	// set next output byte should be put there
	inline U8* NextOut();							// get next output byte should be put there

	inline void SetAvailOut(int nb);				// set remaining free space at next_out
	inline int AvailOut();							// get remaining free space at next_out

	inline int TotalOut();							// get total nb of bytes output so far

private:
	z_stream m_zstream;
};


/////////////////////////////////////////////////////////////////////////////


PlatformZlibStream::PlatformZlibStream()
{
	memset(&m_zstream, 0, sizeof m_zstream);
	int error = inflateInit(&m_zstream);
	(void)error;
	PLAYERASSERT(error == Z_OK);
}


PlatformZlibStream::~PlatformZlibStream()
{
	int error = inflateEnd(&m_zstream);
	(void)error;
	PLAYERASSERT(error == Z_OK);
}


void PlatformZlibStream::SetNextIn(const U8* pbCompressed)
{
	m_zstream.next_in = (Bytef*) pbCompressed;
}


void PlatformZlibStream::SetAvailIn(int nb)
{
	PLAYERASSERT(nb >= 0);
	m_zstream.avail_in = nb;
}


int PlatformZlibStream::AvailIn()
{
	PLAYERASSERT(m_zstream.avail_in >= 0);
	return m_zstream.avail_in;
}


bool PlatformZlibStream::Inflate()
{
	int error = inflate(&m_zstream, Z_NO_FLUSH);
	PLAYERASSERT(error == Z_OK || error == Z_STREAM_END);
	return (error == Z_OK);
}

int PlatformZlibStream::InflateWithStatus()
{
	return inflate(&m_zstream, Z_NO_FLUSH);
}


void PlatformZlibStream::SetNextOut(U8* pbDecompressed)
{
	m_zstream.next_out = (Bytef*) pbDecompressed;
}


U8* PlatformZlibStream::NextOut()
{
	return m_zstream.next_out;
}


void PlatformZlibStream::SetAvailOut(int nb)
{
	PLAYERASSERT(nb >= 0);
	m_zstream.avail_out = nb;
}


int PlatformZlibStream::AvailOut()
{
	PLAYERASSERT(m_zstream.avail_out >= 0);
	return m_zstream.avail_out;
}


int PlatformZlibStream::TotalOut()
{
	PLAYERASSERT(m_zstream.total_out >= 0);
	return m_zstream.total_out;
}

/////////////////////////////////////////////////////////////////////////////
}

#endif // GENERIC_ZLIB_H
