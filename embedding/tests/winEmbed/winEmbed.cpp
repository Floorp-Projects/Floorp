/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Doug Turner <dougt@netscape.com> 
 */


#include "stdafx.h"
#include "resource.h"



#include "WebBrowser.h"

WebBrowser         *mozBrowser = nsnull;

extern nsresult NS_InitEmbedding(const char *aPath);
extern nsresult NS_TermEmbedding();



#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];								// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];								// The title bar text

// Foward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	OpenURI(HWND, UINT, WPARAM, LPARAM);


int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	MSG msg;
	
	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_WINEMBED, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

    NS_InitEmbedding(nsnull);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
	    TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
    
    NS_TermEmbedding();

	return msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_WINEMBED);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCSTR)IDC_WINEMBED;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow( szWindowClass, 
                        szTitle, 
                        WS_OVERLAPPEDWINDOW,
                        0, 
                        0, 
                        450, 
                        450, 
                        NULL, 
                        NULL, 
                        hInstance, 
                        NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   

    mozBrowser = new WebBrowser();
    if (! mozBrowser)
        return FALSE;

    NS_ADDREF(mozBrowser);

    if ( NS_FAILED( mozBrowser->Init(hWnd) ) )
        return FALSE;

    RECT rect;
    GetClientRect(hWnd, &rect);
    rect.top += 32;

    mozBrowser->SetPositionAndSize(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, true);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	TCHAR szHello[MAX_LOADSTRING];
	LoadString(hInst, IDS_HELLO, szHello, MAX_LOADSTRING);

	switch (message) 
	{
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{
                case IDM_EXIT:
				   DestroyWindow(hWnd);
				   break;
                
                case MOZ_Open:
                   DialogBox(hInst, (LPCTSTR)MOZ_OpenURI, hWnd, (DLGPROC)OpenURI);
				   break;
				
                case MOZ_Print:
                    if (mozBrowser)
                        mozBrowser->Print();
                    break;

                default:
				   return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;

        case WM_SIZE:
            RECT rect;
            GetClientRect(hWnd, &rect);
            rect.top += 32;
            if (mozBrowser)
                mozBrowser->SetPositionAndSize(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, true);
            break;
        
		case WM_PAINT:
			
            // this draws that silly text at the top of the window.
            hdc = BeginPaint(hWnd, &ps);
			RECT rt;
			GetClientRect(hWnd, &rt);
            rt.bottom = 32;

            FrameRect(hdc,  &rt, CreateSolidBrush( 0x00 ) );

            rt.top = 4;

			DrawText(hdc, szHello, strlen(szHello), &rt, DT_CENTER);
            EndPaint(hWnd, &ps);
			break;


		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

// Mesage handler for about box.
LRESULT CALLBACK OpenURI(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
				return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
            {
                char szBuf[1000];
                GetDlgItemText(hDlg, MOZ_EDIT_URI, szBuf, 1000);
                EndDialog(hDlg, LOWORD(wParam));
                
                mozBrowser->GoTo(szBuf);
                
                return TRUE;

            }
            else if (LOWORD(wParam) == IDNO) 
            {
                EndDialog(hDlg, LOWORD(wParam));
                return TRUE;
            }
                
			break;
	}
    return FALSE;
}

