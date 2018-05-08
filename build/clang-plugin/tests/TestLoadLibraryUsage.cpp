#include <windows.h>
#include "prlink.h"

void Func() {
  auto h1 = PR_LoadLibrary(nullptr); // expected-error {{Usage of ASCII file functions (such as PR_LoadLibrary) is forbidden.}}
  auto h2 = PR_LoadLibrary("C:\\Some\\Path");
  auto h3 = LoadLibraryA(nullptr); // expected-error {{Usage of ASCII file functions (such as LoadLibraryA) is forbidden.}}
  auto h4 = LoadLibraryA("C:\\Some\\Path");
  auto h5 = LoadLibraryExA(nullptr, nullptr, 0); // expected-error {{Usage of ASCII file functions (such as LoadLibraryExA) is forbidden.}}
  auto h6 = LoadLibraryExA("C:\\Some\\Path", nullptr, 0);

#ifndef UNICODE
  // LoadLibrary is a defnine for LoadLibraryA
  auto h7 = LoadLibrary(nullptr); // expected-error {{Usage of ASCII file functions (such as LoadLibraryA) is forbidden.}}
  auto h8 = LoadLibrary("C:\\Some\\Path");
  // LoadLibraryEx is a define for LoadLibraryExA
  auto h9 = LoadLibraryEx(nullptr, nullptr, 0); // expected-error {{Usage of ASCII file functions (such as LoadLibraryExA) is forbidden.}}
  auto h10 = LoadLibraryEx("C:\\Some\\Path", nullptr, 0);
#endif
}