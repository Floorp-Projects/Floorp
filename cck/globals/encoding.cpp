#include "encoding.h"

// For supporting customization with non-ascii characters through Cecko,
// the encoding of cck.che is changed to UTF8. Widget values are converted
// to and fro between native encoding and UTF8 encoding. That is, the 
// native text entered in the wizard are saved to in-memory copy as UTF8,
// and when displayed in the wizard they are converted back to native 
// encoding. 
// For customizing the Windows system text strings like program folder
// name, additional components and CD autorun strings, encoding is 
// changed from UTF8 back to native encoding.


// Convert native encoding to UTF8 encoding
__declspec(dllexport)
CString ConvertANSItoUTF8(CString strAnsi)
{
	int wLen, mLen, retval;
	CString strUtf8;

	wLen = MultiByteToWideChar(CP_ACP, 0, strAnsi, -1, NULL, 0);
	if (wLen == 0)
	{
		ShowLastError();
		return strAnsi;
	}
	LPWSTR strWidechar = new WCHAR[wLen];
	retval = MultiByteToWideChar(CP_ACP, 0, strAnsi, -1, strWidechar, wLen); 
	if (retval == 0)
	{
		ShowLastError();
		delete [] strWidechar;
		return strAnsi;
	}

	mLen = WideCharToMultiByte(CP_UTF8, 0, strWidechar, -1, NULL, 0, NULL, NULL);
	if (mLen == 0)
	{
		ShowLastError();
		delete [] strWidechar;
		return strAnsi;
	}
	LPSTR strMultibyte = new TCHAR[mLen];
	retval = WideCharToMultiByte(CP_UTF8, 0, strWidechar, -1, strMultibyte, 
		mLen, NULL, NULL);
	if (retval == 0)
	{
		ShowLastError();
		delete [] strWidechar;
		delete [] strMultibyte;
		return strAnsi;
	}

	strUtf8 = strMultibyte;
	delete [] strWidechar;
	delete [] strMultibyte;
	return strUtf8;
}

// Convert UTF8 encoding to native encoding
__declspec(dllexport)
CString ConvertUTF8toANSI(CString strUtf8)
{
	int wLen, mLen, retval;
	CString strAnsi;
	wLen = MultiByteToWideChar(CP_UTF8, 0, strUtf8, -1, NULL, 0);
	if (wLen == 0)
	{
		ShowLastError();
		return strUtf8;
	}
	LPWSTR strWidechar = new WCHAR[wLen];		
	retval = MultiByteToWideChar(CP_UTF8, 0, strUtf8, -1, strWidechar, wLen); 
	if (retval == 0)
	{
		ShowLastError();
		delete [] strWidechar;
		return strUtf8;
	}

	mLen = WideCharToMultiByte(CP_ACP, 0, strWidechar, -1, NULL, 0, NULL, NULL);
	if (mLen == 0)
	{
		ShowLastError();
		delete [] strWidechar;
		return strUtf8;
	}
	LPSTR strMultibyte = new TCHAR[mLen];
	retval = WideCharToMultiByte(CP_ACP, 0, strWidechar, -1, strMultibyte, 
		mLen, NULL, NULL);
	if (retval == 0)
	{
		ShowLastError();
		delete [] strWidechar;
		delete [] strMultibyte;
		return strUtf8;
	}

	strAnsi = strMultibyte;
	delete [] strWidechar;
	delete [] strMultibyte;
	return strAnsi;
}

// Display error message
extern "C" __declspec(dllexport)
void ShowLastError()
{
	char* lpMsgBuf;
	FormatMessage( 
          FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
          NULL,
          GetLastError(),
          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
          (LPTSTR) &lpMsgBuf,
          0,
          NULL);      
	MessageBox( NULL, lpMsgBuf, "Error", MB_OK|MB_ICONINFORMATION );
	LocalFree(lpMsgBuf);
}
