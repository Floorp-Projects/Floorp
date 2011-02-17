//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

// esUtil_win.h
//
//   API-neutral interface for creating windows.  Implementation needs to be provided per-platform.

#ifndef ESUTIL_WIN_H
#define ESUTIL_WIN_H

///
//  Includes
//

#ifdef __cplusplus

extern "C" {
#endif

   
///
//  Macros
//

///
// Types
//

///
//  Public Functions
//

///
//  WinCreate()
//
//      Create Win32 instance and window
//
GLboolean WinCreate ( ESContext *esContext, LPCTSTR title );

///
//  WinLoop()
//
//      Start main windows loop
//
void WinLoop ( ESContext *esContext );

///
//  WinTGALoad()
//    
//      TGA loader win32 implementation
//
int WinTGALoad ( const char *fileName, char **buffer, int *width, int *height );

#ifdef __cplusplus
}
#endif

#endif // ESUTIL_WIN_H
