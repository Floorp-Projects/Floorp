#include "stdafx.h"
#include "WizardTypes.h"
#include "winbase.h"  // for CopyDir
#include <direct.h>

__declspec(dllexport) WIDGET GlobalWidgetArray[1000];
__declspec(dllexport) int GlobalArrayIndex=0;

extern "C" __declspec(dllexport)
WIDGET* findWidget(CString theName)
{
	
	for (int i = 0; i < GlobalArrayIndex; i++)
	{
		if (GlobalWidgetArray[i].name == theName) {
			return (&GlobalWidgetArray[i]);
		}
	}

	return NULL;
}

extern "C" __declspec(dllexport)
WIDGET* SetGlobal(CString theName, CString theValue)
{
	WIDGET* w = findWidget(theName);
	if (w == NULL)
	{
		// Make sure we can add this value
		if (GlobalArrayIndex >= sizeof(GlobalWidgetArray))
			exit(11);

		GlobalWidgetArray[GlobalArrayIndex].name  = theName;
		GlobalWidgetArray[GlobalArrayIndex].value = theValue;
		w = &GlobalWidgetArray[GlobalArrayIndex];
		GlobalArrayIndex++;
	}
	else 
		w->value = theValue;

	return w;
}

extern "C" __declspec(dllexport)
char *GetGlobal(CString theName)
{
	WIDGET *w = findWidget(theName);

	if (w)
		return (char *) (LPCTSTR) w->value;

	return "";
}

extern "C" __declspec(dllexport)
void CopyDir(CString from, CString to, CString extension)
{
	WIN32_FIND_DATA data;
	HANDLE d;
	CString dot = ".";
	CString dotdot = "..";
	CString fchild, tchild;
	CString pattern = from + "\\*.*";
	int		found;
	DWORD	tmp;


	d = FindFirstFile((const char *) to, &data);
	if (d == INVALID_HANDLE_VALUE)
		mkdir(to);

	d = FindFirstFile((const char *) pattern, &data);
	found = (d != INVALID_HANDLE_VALUE);

	while (found)
	{
		if (data.cFileName != dot && data.cFileName != dotdot)
		{
			fchild = from + "\\" + data.cFileName;
			tchild = to + "\\" + data.cFileName;
			tmp = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			if (tmp == FILE_ATTRIBUTE_DIRECTORY)
				CopyDir(fchild, tchild,"NULL");
			else
			{
				CString spot=fchild;
				int loc = fchild.Find('.');
				if (loc)
					spot.Delete(0,loc+1);
				if ((spot.CompareNoCase(extension)==0) || (extension.Compare("NULL") ==0))
					CopyFile((const char *) fchild, (const char *) tchild, FALSE);
			}									
		}

		found = FindNextFile(d, &data);
	}

	FindClose(d);
}

