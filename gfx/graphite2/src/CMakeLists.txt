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
set(GRAPHITE_API_REVISION 2)
set(GRAPHITE_API_AGE 1)
set(GRAPHITE_VERSION ${GRAPHITE_API_CURRENT}.${GRAPHITE_API_REVISION}.${GRAPHITE_API_AGE})
set(GRAPHITE_SO_VERSION ${GRAPHITE_API_CURRENT})

include(TestBigEndian)

include_directories(${PROJECT_SOURCE_DIR})

set(FILEFACE FileFace.cpp)
if (GRAPHITE2_NFILEFACE)
    add_definitions(-DGRAPHITE2_NFILEFACE)
    set(FILEFACE)
endif()

set(TRACING json.cpp)
if (GRAPHITE2_NTRACING)
    add_definitions(-DGRAPHITE2_NTRACING)
    set(TRACING)
endif()

if (GRAPHITE2_TELEMETRY)
    add_definitions(-DGRAPHITE2_TELEMETRY)
endif()

if (NOT BUILD_SHARED_LIBS)
    add_definitions(-DGRAPHITE2_STATIC)
endif()

set(GRAPHITE_HEADERS
    ../include/graphite2/Font.h
    ../include/graphite2/Segment.h
    ../include/graphite2/Types.h
    ../include/graphite2/Log.h
    )

file(GLOB PRIVATE_HEADERS inc/*.h)

add_library(graphite2
    ${GRAPHITE2_VM_TYPE}_machine.cpp
    gr_char_info.cpp
    gr_features.cpp
    gr_face.cpp
    gr_font.cpp
    gr_logging.cpp
    gr_segment.cpp
    gr_slot.cpp
    CmapCache.cpp
    Code.cpp
    Collider.cpp
    Decompressor.cpp
    Face.cpp
    FeatureMap.cpp
    Font.cpp
    GlyphFace.cpp
    GlyphCache.cpp
    Intervals.cpp
    Justifier.cpp
    NameTable.cpp
    Pass.cpp
    Position.cpp
    Segment.cpp
    Silf.cpp
    Slot.cpp
    Sparse.cpp
    TtfUtil.cpp
    UtfCodec.cpp
    ${FILEFACE}
    ${TRACING})

set_target_properties(graphite2 PROPERTIES  PUBLIC_HEADER "${GRAPHITE_HEADERS}"
                                            SOVERSION ${GRAPHITE_SO_VERSION}
                                            VERSION ${GRAPHITE_VERSION}
                                            LT_VERSION_CURRENT ${GRAPHITE_API_CURRENT}
                                            LT_VERSION_REVISION ${GRAPHITE_API_REVISION}
                                            LT_VERSION_AGE ${GRAPHITE_API_AGE})

if  (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set_target_properties(graphite2 PROPERTIES
        COMPILE_FLAGS   "-Wall -Wextra -Wno-unknown-pragmas -Wendif-labels -Wshadow -Wctor-dtor-privacy -Wnon-virtual-dtor -fno-rtti -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden"
        LINK_FLAGS      "-nodefaultlibs ${GRAPHITE_LINK_FLAGS}"
        LINKER_LANGUAGE C)
    if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86|i.86")
        add_definitions(-mfpmath=sse -msse2)
    endif()
    if (CMAKE_COMPILER_IS_GNUCXX)
        add_definitions(-Wno-class-memaccess -Wdouble-promotion)
    endif()
    message(STATUS "Compiler ID is: ${CMAKE_CXX_COMPILER_ID}")
    if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
        add_definitions(-Wimplicit-fallthrough -Wshorten-64-to-32)
    endif()
    if (${CMAKE_CXX_COMPILER} MATCHES  ".*mingw.*")
        target_link_libraries(graphite2 kernel32 msvcr90 mingw32 gcc user32)
    else (${CMAKE_CXX_COMPILER} MATCHES  ".*mingw.*")
        if (GRAPHITE2_SANITIZERS)
            target_link_libraries(graphite2 c gcc_s)
        else ()
            target_link_libraries(graphite2 c gcc)
        endif ()
    endif()
    include(Graphite)
    if (BUILD_SHARED_LIBS)
        nolib_test(stdc++ $<TARGET_SONAME_FILE:graphite2>)
    endif ()
    set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
    CREATE_LIBTOOL_FILE(graphite2 "/lib${LIB_SUFFIX}")
endif()

if  (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set_target_properties(graphite2 PROPERTIES
        COMPILE_FLAGS   "-Wall -Wextra -Wno-unknown-pragmas -Wimplicit-fallthrough -Wendif-labels -Wshadow -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -fno-rtti -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden"
        LINK_FLAGS      "-nodefaultlibs"
        LINKER_LANGUAGE C)
    if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86|i.86")
        add_definitions(-mfpmath=sse -msse2)
    endif()
    target_link_libraries(graphite2 c)
    include(Graphite)
    nolib_test(stdc++ $<TARGET_SONAME_FILE:graphite2>)
    set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
    CREATE_LIBTOOL_FILE(graphite2 "/lib${LIB_SUFFIX}")
endif()

if  (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set_target_properties(graphite2 PROPERTIES
        COMPILE_DEFINITIONS "_SCL_SECURE_NO_WARNINGS;_CRT_SECURE_NO_WARNINGS;UNICODE;GRAPHITE2_EXPORTING")
endif()


install(TARGETS graphite2 EXPORT graphite2 LIBRARY DESTINATION lib${LIB_SUFFIX} ARCHIVE DESTINATION lib${LIB_SUFFIX} PUBLIC_HEADER DESTINATION include/graphite2 RUNTIME DESTINATION bin)
install(EXPORT graphite2 DESTINATION share/graphite2 NAMESPACE gr2_)
