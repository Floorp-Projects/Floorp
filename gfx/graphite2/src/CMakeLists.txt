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

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.0 FATAL_ERROR)
project(graphite2_core)
cmake_policy(SET CMP0012 NEW)
INCLUDE(CheckCXXSourceCompiles)

set(GRAPHITE_API_MAJOR 2)
set(GRAPHITE_API_MINOR 0)
set(GRAPHITE_API_AGE 0)
set(GRAPHITE_SO_VERSION ${GRAPHITE_API_MAJOR}.${GRAPHITE_API_MINOR}.${GRAPHITE_API_AGE})

include(TestBigEndian)

include_directories(${PROJECT_SOURCE_DIR})

if (DISABLE_SEGCACHE)
    add_definitions(-DDISABLE_SEGCACHE)
endif (DISABLE_SEGCACHE)

if (DISABLE_FILEFACE)
    add_definitions(-DDISABLE_FILE_FACE)
endif (DISABLE_FILEFACE)

if (NOT DISABLE_TRACING)
    set(TRACE_HEADERS ../include/graphite2/XmlLog.h)
    if (ENABLE_DEEP_TRACING)
        add_definitions(-DENABLE_DEEP_TRACING)
    endif (ENABLE_DEEP_TRACING)
endif (NOT DISABLE_TRACING)

set(GRAPHITE_HEADERS 
    ../include/graphite2/Font.h
    ../include/graphite2/Segment.h
    ../include/graphite2/Types.h
    ${TRACE_HEADERS}
    )

file(GLOB PRIVATE_HEADERS *.h) 

add_library(graphite2 SHARED
    ${VM_MACHINE_TYPE}_machine.cpp
    gr_char_info.cpp
    gr_features.cpp
    gr_face.cpp
    gr_font.cpp
    gr_logging.cpp
    gr_segment.cpp
    gr_slot.cpp
    Bidi.cpp
    CachedFace.cpp
    CmapCache.cpp
    Code.cpp
    Face.cpp
    FeatureMap.cpp
    Font.cpp
    GlyphFace.cpp
    GlyphFaceCache.cpp
    NameTable.cpp
    Pass.cpp
    SegCache.cpp
    SegCacheEntry.cpp
    SegCacheStore.cpp
    Segment.cpp
    Silf.cpp
    Slot.cpp
    Sparse.cpp
    TtfUtil.cpp
    UtfCodec.cpp
    XmlTraceLog.cpp
    XmlTraceLogTags.cpp)

set_target_properties(graphite2 PROPERTIES  PUBLIC_HEADER "${GRAPHITE_HEADERS}"
                                            SOVERSION ${GRAPHITE_SO_VERSION}
                                            LT_VERSION_CURRENT ${GRAPHITE_API_MAJOR}
                                            LT_VERSION_REVISION ${GRAPHITE_API_MINOR}
                                            LT_VERSION_AGE ${GRAPHITE_API_AGE})

if  (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    add_definitions(-Wall -Wno-unknown-pragmas -Wparentheses -Wextra -Wendif-labels
     -Wshadow -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -fdiagnostics-show-option
     -fno-rtti -fno-exceptions -nodefaultlibs
     -fvisibility=hidden -fvisibility-inlines-hidden -fno-stack-protector)
    set_target_properties(graphite2 PROPERTIES LINK_FLAGS "-nodefaultlibs" LINKER_LANGUAGE C)
    if (${CMAKE_CXX_COMPILER} MATCHES  ".*mingw.*")
        target_link_libraries(graphite2 "-lkernel32 -lmsvcr90 -lmingw32 -lgcc -luser32")
    else (${CMAKE_CXX_COMPILER} MATCHES  ".*mingw.*")
        target_link_libraries(graphite2 "-lc -lgcc")
        include(Graphite)
        nolib_test(stdc++ $<TARGET_SONAME_FILE:graphite2>)
    endif (${CMAKE_CXX_COMPILER} MATCHES  ".*mingw.*")
    set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
    CREATE_LIBTOOL_FILE(graphite2 "/lib${LIB_SUFFIX}")
endif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

if  (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -DUNICODE -DGR2_EXPORTING)
endif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")


install(TARGETS graphite2 EXPORT graphite2 LIBRARY DESTINATION lib${LIB_SUFFIX} ARCHIVE DESTINATION lib${LIB_SUFFIX} PUBLIC_HEADER DESTINATION include/graphite2 RUNTIME DESTINATION bin)
install(EXPORT graphite2 DESTINATION share/graphite2 NAMESPACE gr2_)