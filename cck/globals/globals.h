#include "stdafx.h"
#include "WizardTypes.h"

extern __declspec(dllimport) WIDGET 	GlobalWidgetArray[];
extern __declspec(dllimport) int 		GlobalArrayIndex;
extern __declspec(dllimport) BOOL IsSameCache;


extern "C" __declspec(dllimport) char  * GetGlobal(CString theName);
extern "C" __declspec(dllimport) WIDGET* SetGlobal(CString theName, CString theValue);
extern "C" __declspec(dllimport) WIDGET* findWidget(CString theName);
extern "C" __declspec(dllimport) void CopyDir(CString from, CString to, LPCTSTR extension, int overwrite);
extern "C" __declspec(dllexport) void ExecuteCommand(char *command, int showflag, DWORD wait);
