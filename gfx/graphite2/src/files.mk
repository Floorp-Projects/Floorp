#    GRAPHITE2 LICENSING
#
#    Copyright 2010, SIL International
#    All rights reserved.
#
#    This library is free software; you can redistribute it and/or modify
#    it under the terms of the GNU Lesser General Public License as published
#    by the Free Software Foundation; either version 2.1 of License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#    Lesser General Public License for more details.
#
#    You should also have received a copy of the GNU Lesser General Public
#    License along with this library in the file named "LICENSE".
#    If not, write to the Free Software Foundation, 51 Franklin Street, 
#    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the 
#    internet at http://www.fsf.org/licenses/lgpl.html.
#
# Alternatively, the contents of this file may be used under the terms of the
# Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
# License, as published by the Free Software Foundation, either version 2
# of the License or (at your option) any later version.

# Makefile helper file for those wanting to build Graphite2 using make
# The including makefile should set the following variables
# _NS               Prefix to all variables this file creates (namespace)
# $(_NS)_MACHINE    Set to direct or call. Set to direct if using gcc else
#                   set to call
# $(_NS)_BASE       path to root of graphite2 project
#
# Returns:
# $(_NS)_SOURCES    List of source files (with .cpp extension)
# $(_NS)_PRIVATE_HEADERS    List of private header files (with .h extension)
# $(_NS)_PUBLIC_HEADERS     List of public header files (with .h extension)


$(_NS)_SOURCES = \
    $($(_NS)_BASE)/src/$($(_NS)_MACHINE)_machine.cpp \
    $($(_NS)_BASE)/src/gr_char_info.cpp \
    $($(_NS)_BASE)/src/gr_features.cpp \
    $($(_NS)_BASE)/src/gr_face.cpp \
    $($(_NS)_BASE)/src/gr_font.cpp \
    $($(_NS)_BASE)/src/gr_segment.cpp \
    $($(_NS)_BASE)/src/gr_slot.cpp \
    $($(_NS)_BASE)/src/Bidi.cpp \
    $($(_NS)_BASE)/src/CachedFace.cpp \
    $($(_NS)_BASE)/src/CmapCache.cpp \
    $($(_NS)_BASE)/src/Code.cpp \
    $($(_NS)_BASE)/src/Face.cpp \
    $($(_NS)_BASE)/src/FeatureMap.cpp \
    $($(_NS)_BASE)/src/Font.cpp \
    $($(_NS)_BASE)/src/GlyphFace.cpp \
    $($(_NS)_BASE)/src/GlyphFaceCache.cpp \
    $($(_NS)_BASE)/src/NameTable.cpp \
    $($(_NS)_BASE)/src/Pass.cpp \
    $($(_NS)_BASE)/src/SegCache.cpp \
    $($(_NS)_BASE)/src/SegCacheEntry.cpp \
    $($(_NS)_BASE)/src/SegCacheStore.cpp \
    $($(_NS)_BASE)/src/Segment.cpp \
    $($(_NS)_BASE)/src/Silf.cpp \
    $($(_NS)_BASE)/src/Slot.cpp \
    $($(_NS)_BASE)/src/Sparse.cpp \
    $($(_NS)_BASE)/src/TtfUtil.cpp \
    $($(_NS)_BASE)/src/UtfCodec.cpp

$(_NS)_PRIVATE_HEADERS = \
    $($(_NS)_BASE)/src/CachedFace.h \
    $($(_NS)_BASE)/src/CharInfo.h \
    $($(_NS)_BASE)/src/CmapCache.h \
    $($(_NS)_BASE)/src/Code.h \
    $($(_NS)_BASE)/src/Face.h \
    $($(_NS)_BASE)/src/FeatureMap.h \
    $($(_NS)_BASE)/src/FeatureVal.h \
    $($(_NS)_BASE)/src/Font.h \
    $($(_NS)_BASE)/src/GlyphFaceCache.h \
    $($(_NS)_BASE)/src/GlyphFace.h \
    $($(_NS)_BASE)/src/List.h \
    $($(_NS)_BASE)/src/locale2lcid.h \
    $($(_NS)_BASE)/src/Machine.h \
    $($(_NS)_BASE)/src/Main.h \
    $($(_NS)_BASE)/src/NameTable.h \
    $($(_NS)_BASE)/src/opcodes.h \
    $($(_NS)_BASE)/src/opcode_table.h \
    $($(_NS)_BASE)/src/Pass.h \
    $($(_NS)_BASE)/src/Position.h \
    $($(_NS)_BASE)/src/processUTF.h \
    $($(_NS)_BASE)/src/Rule.h \
    $($(_NS)_BASE)/src/SegCacheEntry.h \
    $($(_NS)_BASE)/src/SegCache.h \
    $($(_NS)_BASE)/src/SegCacheStore.h \
    $($(_NS)_BASE)/src/Segment.h \
    $($(_NS)_BASE)/src/Silf.h \
    $($(_NS)_BASE)/src/Slot.h \
    $($(_NS)_BASE)/src/Sparse.h \
    $($(_NS)_BASE)/src/TtfTypes.h \
    $($(_NS)_BASE)/src/TtfUtil.h \
    $($(_NS)_BASE)/src/UtfCodec.h \
    $($(_NS)_BASE)/src/XmlTraceLog.h \
    $($(_NS)_BASE)/src/XmlTraceLogTags.h 

$(_NS)_PUBLIC_HEADERS = \
    $($(_NS)_BASE)/include/graphite2/Font.h \
    $($(_NS)_BASE)/include/graphite2/Segment.h \
    $($(_NS)_BASE)/include/graphite2/Types.h \
    $($(_NS)_BASE)/include/graphite2/XmlLog.h

