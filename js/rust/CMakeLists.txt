project(rust-mozjs)
cmake_minimum_required(VERSION 2.6)

set(DUMMY ${CMAKE_BUILD_TYPE})

set(SOURCES
  src/jsglue.cpp
  )

include_directories($ENV{DEP_MOZJS_OUTDIR}/dist/include)

if(MSVC)
  if(NOT "$ENV{CARGO_FEATURE_DEBUGMOZJS}" STREQUAL "")
    add_definitions(-MDd -Od -DDEBUG -D_DEBUG)
  else()
    add_definitions(-MD)
  endif()
  add_definitions(-FI$ENV{DEP_MOZJS_OUTDIR}/js/src/js-confdefs.h)
  add_definitions(-DWIN32)
  add_definitions(-Zi -GR-)
else()
  if(NOT "$ENV{CARGO_FEATURE_DEBUGMOZJS}" STREQUAL "")
    add_definitions(-g -O0 -DDEBUG -D_DEBUG)
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_definitions(-Wno-c++0x-extensions -Wno-return-type-c-linkage -Wno-invalid-offsetof)
  endif()
  add_definitions(-fPIC -fno-rtti)
  add_definitions(-std=c++14 -DJS_NO_JSVAL_JSID_STRUCT_TYPES)
  add_definitions(-include $ENV{DEP_MOZJS_OUTDIR}/js/src/js-confdefs.h)
endif()

add_library(jsglue STATIC ${SOURCES})
install(TARGETS jsglue ARCHIVE DESTINATION lib)

