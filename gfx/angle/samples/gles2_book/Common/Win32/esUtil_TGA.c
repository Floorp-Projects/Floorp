//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

// esUtil_TGA.c
//
//    This file contains the Win32 implementation of a TGA image loader

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

///
//  Macros
//
#define INVERTED_BIT            (1 << 5)

///
//  Types
//
#pragma pack(push,x1)                            // Byte alignment (8-bit)
#pragma pack(1)

typedef struct
{
   unsigned char  IdSize,
                  MapType,
                  ImageType;
   unsigned short PaletteStart,
                  PaletteSize;
   unsigned char  PaletteEntryDepth;
   unsigned short X,
                  Y,
                  Width,
                  Height;
   unsigned char  ColorDepth,
                  Descriptor;
         
} TGA_HEADER;

#pragma pack(pop,x1)

////////////////////////////////////////////////////////////////////////////////////
//
//  Private Functions
//

////////////////////////////////////////////////////////////////////////////////////
//
//  Public Functions
//
//


///
//  WinTGALoad()
//
int WinTGALoad( const char *fileName, char **buffer, int *width, int *height )
{
   FILE        *fp;
   TGA_HEADER   Header;

   if ( fopen_s ( &fp, fileName, "rb" ) != 0 )
   {
      return FALSE;
   }

   if ( fp == NULL )
   {
      return FALSE;
   }

   fread ( &Header, sizeof(TGA_HEADER), 1, fp );

   *width = Header.Width;
   *height = Header.Height;
   
   if ( Header.ColorDepth == 24 )
   {
      RGBTRIPLE *Buffer24;

      Buffer24= (RGBTRIPLE*)malloc(sizeof(RGBTRIPLE) * (*width) * (*height));

      if(Buffer24)
      {
         int i=0;
         int x,
             y;

         fread(Buffer24, sizeof(RGBTRIPLE), (*width) * (*height), fp);

         *buffer= (LPSTR) malloc(3 * (*width) * (*height));

         for ( y = 0; y < *height; y++ )
            for( x = 0; x < *width; x++ )
            {
               int Index= y * (*width) + x;

               if(!(Header.Descriptor & INVERTED_BIT))
                  Index= ((*height) - 1 - y) * (*width) + x;

               (*buffer)[(i * 3)]=      Buffer24[Index].rgbtRed;
               (*buffer)[(i * 3) + 1]=  Buffer24[Index].rgbtGreen;
               (*buffer)[(i * 3) + 2]=  Buffer24[Index].rgbtBlue;
        
               i++;
            }
         
         fclose(fp);
         free(Buffer24);
         return(TRUE);
      }		
   }

   return(FALSE);
}
