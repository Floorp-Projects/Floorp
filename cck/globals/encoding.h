#include "stdafx.h"

extern "C" __declspec(dllexport) void ShowLastError();
__declspec(dllexport) CString ConvertANSItoUTF8(CString strAnsi);
__declspec(dllexport) CString ConvertUTF8toANSI(CString strUtf8);
