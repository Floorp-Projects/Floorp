#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <fstream>
#include <windows.h>

void func_fopen() {
  FILE *f1 = fopen("dummy.txt", "rt"); // expected-warning {{Usage of ASCII file functions (here fopen) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}
  FILE *f2;
  fopen_s(&f2, "dummy.txt", "rt"); // expected-warning {{Usage of ASCII file functions (here fopen_s) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}

  int fh1 = _open("dummy.txt", _O_RDONLY); // expected-warning {{Usage of ASCII file functions (here _open) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}
  int fh2 = open("dummy.txt", _O_RDONLY); // expected-warning {{Usage of ASCII file functions (here open) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}
  int fh3 = _sopen("dummy.txt", _O_RDONLY, _SH_DENYRW); // expected-warning {{Usage of ASCII file functions (here _sopen) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}
  int fd4;
  errno_t err = _sopen_s(&fd4, "dummy.txt", _O_RDONLY, _SH_DENYRW, 0); // expected-warning {{Usage of ASCII file functions (here _sopen_s) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}

  std::fstream fs1;
  fs1.open("dummy.txt"); // expected-warning {{Usage of ASCII file functions (here open) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}
  std::ifstream ifs1;
  ifs1.open("dummy.txt"); // expected-warning {{Usage of ASCII file functions (here open) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}
  std::ofstream ofs1;
  ofs1.open("dummy.txt"); // expected-warning {{Usage of ASCII file functions (here open) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}
#ifdef _MSC_VER
  std::fstream fs2;
  fs2.open(L"dummy.txt");
  std::ifstream ifs2;
  ifs2.open(L"dummy.txt");
  std::ofstream ofs2;
  ofs2.open(L"dummy.txt");
#endif

  LPOFSTRUCT buffer;
  HFILE hFile1 = OpenFile("dummy.txt", buffer, OF_READ); // expected-warning {{Usage of ASCII file functions (here OpenFile) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}

#ifndef UNICODE
  // CreateFile is just an alias of CreateFileA
  LPCSTR buffer2;
  HANDLE hFile2 = CreateFile(buffer2, GENERIC_WRITE, 0, NULL, CREATE_NEW, // expected-warning {{Usage of ASCII file functions (here CreateFileA) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}
	  FILE_ATTRIBUTE_NORMAL, NULL);
#else
  // CreateFile is just an alias of CreateFileW and should not be matched
  LPCWSTR buffer2;
  HANDLE hFile2 = CreateFile(buffer2, GENERIC_WRITE, 0, NULL, CREATE_NEW,
	  FILE_ATTRIBUTE_NORMAL, NULL);
#endif
  LPCSTR buffer3;
  HANDLE hFile3 = CreateFileA(buffer3, GENERIC_WRITE, 0, NULL, CREATE_NEW, // expected-warning {{Usage of ASCII file functions (here CreateFileA) is forbidden on Windows.}} expected-note {{On Windows executed functions: fopen, fopen_s, open, _open, _sopen, _sopen_s, OpenFile, CreateFileA should never be used due to lossy conversion from UTF8 to ANSI.}}
	  FILE_ATTRIBUTE_NORMAL, NULL);
}
