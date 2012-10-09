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

set(GRAPHITE_API_CURRENT 3)
set(GRAPHITE_API_REVISION 0)
set(GRAPHITE_API_AGE 1)
set(GRAPHITE_VERSION ${GRAPHITE_API_CURRENT}.${GRAPHITE_API_REVISION}.${GRAPHITE_API_AGE})
set(GRAPHITE_SO_VERSION ${GRAPHITE_API_CURRENT})

include(TestBigEndian)

include_directories(${PROJECT_SOURCE_DIR})

set(SEGCACHE SegCache.cpp SegCacheEntry.cpp SegCacheStore.cpp)
if (GRAPHITE2_NSEGCACHE)
    add_definitions(-DGRAPHITE2_NSEGCACHE)
    set(SEGCACHE)
endif (GRAPHITE2_NSEGCACHE)

set(FILEFACE FileFace.cpp)
if (GRAPHITE2_NFILEFACE)
    add_definitions(-DGRAPHITE2_NFILEFACE)
    set(FILEFACE)
endif (GRAPHITE2_NFILEFACE)

set(TRACING json.cpp)
if (GRAPHITE2_NTRACING)
    add_definitions(-DGRAPHITE2_NTRACING)
    set(TRACING)
endif (GRAPHITE2_NTRACING)


set(GRAPHITE_HEADERS 
    ../include/graphite2/Font.h
    ../include/graphite2/Segment.h
    ../include/graphite2/Types.h
    ../include/graphite2/Log.h
    )

file(GLOB PRIVATE_HEADERS inc/*.h) 

add_library(graphite2 SHARED
    ${GRAPHITE2_VM_TYPE}_machine.cpp
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
    GlyphCache.cpp
    Justifier.cpp
    NameTable.cpp
    Pass.cpp
    Rule.cpp
    Segment.cpp
    Silf.cpp
    Slot.cpp
    Sparse.cpp
    TtfUtil.cpp
    UtfCodec.cpp
    ${FILEFACE}
    ${SEGCACHE}
    ${TRACING})

set_target_properties(graphite2 PROPERTIES  PUBLIC_HEADER "${GRAPHITE_HEADERS}"
                                            SOVERSION ${GRAPHITE_SO_VERSION}
                                            VERSION ${GRAPHITE_VERSION}
                                            LT_VERSION_CURRENT ${GRAPHITE_API_CURRENT}
                                            LT_VERSION_REVISION ${GRAPHITE_API_REVISION}
                                            LT_VERSION_AGE ${GRAPHITE_API_AGE})

if  (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set_target_properties(graphite2 PROPERTIES 
        COMPILE_FLAGS   "-Wall -Wextra -Wno-unknown-pragmas -Wendif-labels -Wshadow -Wctor-dtor-privacy -Wnon-virtual-dtor -fdiagnostics-show-option -fno-rtti -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden -fno-stack-protector"
        LINK_FLAGS      "-nodefaultlibs" 
        LINKER_LANGUAGE C)
    if (${CMAKE_CXX_COMPILER} MATCHES  ".*mingw.*")
        target_link_libraries(graphite2 kernel32 msvcr90 mingw32 gcc user32)
    else (${CMAKE_CXX_COMPILER} MATCHES  ".*mingw.*")
        target_link_libraries(graphite2 c gcc)
        include(Graphite)
        nolib_test(stdc++ $<TARGET_SONAME_FILE:graphite2>)
    endif (${CMAKE_CXX_COMPILER} MATCHES  ".*mingw.*")
    set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
    CREATE_LIBTOOL_FILE(graphite2 "/lib${LIB_SUFFIX}")
endif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

if  (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set_target_properties(graphite2 PROPERTIES 
        COMPILE_FLAGS   "-Wall -Wextra -Wno-unknown-pragmas -Wendif-labels -Wshadow -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -fdiagnostics-show-option -fno-rtti -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden -fno-stack-protector"
        LINK_FLAGS      "-nodefaultlibs" 
        LINKER_LANGUAGE C)
    target_link_libraries(graphite2 c)
    include(Graphite)
    nolib_test(stdc++ $<TARGET_SONAME_FILE:graphite2>)
    set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
    CREATE_LIBTOOL_FILE(graphite2 "/lib${LIB_SUFFIX}")
endif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")

if  (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set_target_properties(graphite2 PROPERTIES 
        COMPILE_DEFINITIONS "_SCL_SECURE_NO_WARNINGS;_CRT_SECURE_NO_WARNINGS;UNICODE;GRAPHITE2_EXPORTING")
endif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")


install(TARGETS graphite2 EXPORT graphite2 LIBRARY DESTINATION lib${LIB_SUFFIX} ARCHIVE DESTINATION lib${LIB_SUFFIX} PUBLIC_HEADER DESTINATION include/graphite2 RUNTIME DESTINATION bin)
install(EXPORT graphite2 DESTINATION share/graphite2 NAMESPACE gr2_)
