/* ***** BEGIN LICENSE BLOCK *****
 * 
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 * 
 * ***** END LICENSE BLOCK ***** */

#include "nptest_utils.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mozilla/NullPtr.h"

NPUTF8*
createCStringFromNPVariant(const NPVariant* variant)
{
  size_t length = NPVARIANT_TO_STRING(*variant).UTF8Length;
  NPUTF8* result = (NPUTF8*)malloc(length + 1);
  memcpy(result, NPVARIANT_TO_STRING(*variant).UTF8Characters, length);
  result[length] = '\0';
  return result;
}

NPIdentifier
variantToIdentifier(NPVariant variant)
{
  if (NPVARIANT_IS_STRING(variant))
    return stringVariantToIdentifier(variant);
  else if (NPVARIANT_IS_INT32(variant))
    return int32VariantToIdentifier(variant);
  else if (NPVARIANT_IS_DOUBLE(variant))
    return doubleVariantToIdentifier(variant);
  return 0;
}

NPIdentifier
stringVariantToIdentifier(NPVariant variant)
{
  assert(NPVARIANT_IS_STRING(variant));
  NPUTF8* utf8String = createCStringFromNPVariant(&variant);
  NPIdentifier identifier = NPN_GetStringIdentifier(utf8String);
  free(utf8String);
  return identifier;
}

NPIdentifier
int32VariantToIdentifier(NPVariant variant)
{
  assert(NPVARIANT_IS_INT32(variant));
  int32_t integer = NPVARIANT_TO_INT32(variant);
  return NPN_GetIntIdentifier(integer);
}

NPIdentifier
doubleVariantToIdentifier(NPVariant variant)
{
  assert(NPVARIANT_IS_DOUBLE(variant));
  double value = NPVARIANT_TO_DOUBLE(variant);
  // sadly there is no "getdoubleidentifier"
  int32_t integer = static_cast<int32_t>(value);
  return NPN_GetIntIdentifier(integer);
}

/*
 * Parse a color in hex format, #AARRGGBB or AARRGGBB.
 */
uint32_t
parseHexColor(const char* color, int len)
{
  uint8_t bgra[4] = { 0, 0, 0, 0xFF };
  int i = 0;

  // Ignore unsupported formats.
  if (len != 9 && len != 8)
    return 0;

  // start from the right and work to the left
  while (len >= 2) { // we have at least #AA or AA left.
    char byte[3];
    // parse two hex digits
    byte[0] = color[len - 2];
    byte[1] = color[len - 1];
    byte[2] = '\0';

    bgra[i] = (uint8_t)(strtoul(byte, nullptr, 16) & 0xFF);
    i++;
    len -= 2;
  }
  return (bgra[3] << 24) | (bgra[2] << 16) | (bgra[1] << 8) | bgra[0];
}
