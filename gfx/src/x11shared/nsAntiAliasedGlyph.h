
#ifndef NSANTIALIASEDGLYPH_H
#define NSANTIALIASEDGLYPH_H

#include "nscore.h"

struct _XImage;

#ifndef MIN
#define MIN(a,b) (((a) <= (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) (((a) >= (b)) ? (a) : (b))
#endif

#define GLYPH_LEFT_EDGE(m) MIN(0, (m)->lbearing)
#define GLYPH_RIGHT_EDGE(m) MAX((m)->width, (m)->rbearing)

typedef struct _GlyphMetrics {
  PRUint32 width;
  PRUint32 height;
  PRInt32 lbearing;
  PRInt32 rbearing;
  PRInt32 advance;
  PRInt32 ascent;
  PRInt32 descent;
} GlyphMetrics;

//
// Grey scale image
//
class nsAntiAliasedGlyph {
public:
  nsAntiAliasedGlyph(PRUint32 aWidth, PRUint32 aHeight, PRUint32 aBorder);
  ~nsAntiAliasedGlyph();

  PRBool Init();                                      // alloc a buffer
  PRBool Init(PRUint8 *aBuffer, PRUint32 aBufferLen); // use this buffer

  inline PRUint32 GetBorder()       { return mBorder; };
  inline PRUint8 *GetBuffer()       { return mBuffer; };
  inline PRUint32 GetBufferLen()    { return mBufferLen; };
  inline PRUint32 GetBufferWidth()  { return mBufferWidth; };
  inline PRUint32 GetBufferHeight() { return mBufferHeight; };

  inline PRInt32 GetAdvance()       { return mAdvance; };
  inline PRInt32 GetLBearing()      { return mLBearing; };
  inline PRInt32 GetRBearing()      { return mRBearing; };
  inline PRUint32 GetWidth()        { return mWidth; };
  inline PRUint32 GetHeight()       { return mHeight; };

  PRBool SetImage(XCharStruct *, _XImage *);
  PRBool SetSize(GlyphMetrics *);

protected:
  PRUint32 mBorder;
  PRInt32  mAscent;
  PRInt32  mDescent;
  PRInt32  mLBearing;
  PRInt32  mRBearing;
  PRInt32  mAdvance;
  PRUint32 mWidth;
  PRUint32 mHeight;
  PRUint32 mMaxWidth;
  PRUint32 mMaxHeight;
  PRUint32 mBufferWidth;  // mWidth may be smaller
  PRUint32 mBufferHeight; // mHeight may be smaller
  PRBool   mOwnBuffer;
  PRUint8 *mBuffer;
  PRUint32 mBufferLen;
};



#endif /* NSANTIALIASEDGLYPH_H */
