/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsTextFragment_h___
#define nsTextFragment_h___

#include "nslayout.h"
#include "nsAWritableString.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsMemory.h"

// XXX should this normalize the code to keep a \u0000 at the end?

// XXX nsTextFragmentPool?

// XXX these need I18N spankage
#define XP_IS_SPACE(_ch) \
  (((_ch) == ' ') || ((_ch) == '\t') || ((_ch) == '\n'))

#define XP_IS_UPPERCASE(_ch) \
  (((_ch) >= 'A') && ((_ch) <= 'Z'))

#define XP_IS_LOWERCASE(_ch) \
  (((_ch) >= 'a') && ((_ch) <= 'z'))

#define XP_TO_LOWER(_ch) ((_ch) | 32)

#define XP_TO_UPPER(_ch) ((_ch) & ~32)

#define XP_IS_SPACE_W XP_IS_SPACE

/**
 * A fragment of text. If mIs2b is 1 then the m2b pointer is valid
 * otherwise the m1b pointer is valid. If m1b is used then each byte
 * of data represents a single ucs2 character with the high byte being
 * zero.
 *
 * This class does not have a virtual destructor therefore it is not
 * meant to be subclassed.
 */
class NS_LAYOUT nsTextFragment {
public:
  /**
   * Default constructor. Initialize the fragment to be empty.
   */
  nsTextFragment() {
    m1b = nsnull;
    mAllBits = 0;
  }

  ~nsTextFragment() {
    ReleaseText();
  }

  /**
   * Initialize the contents of this fragment to be a copy of
   * the argument fragment.
   */
  nsTextFragment(const nsTextFragment& aOther);

  /**
   * Initialize the contents of this fragment to be a copy of
   * the argument 7bit ascii string.
   */
  nsTextFragment(const char* aString);

  /**
   * Initialize the contents of this fragment to be a copy of
   * the argument ucs2 string.
   */
  nsTextFragment(const PRUnichar* aString) : m1b(nsnull), mAllBits(0) {
    SetTo(aString, nsCRT::strlen(aString));
  }


  /**
   * Initialize the contents of this fragment to be a copy of
   * the argument string.
   */
  nsTextFragment(const nsString& aString);

  /**
   * Change the contents of this fragment to be a copy of the
   * the argument fragment.
   */
  nsTextFragment& operator=(const nsTextFragment& aOther);

  /**
   * Change the contents of this fragment to be a copy of the
   * the argument 7bit ascii string.
   */
  nsTextFragment& operator=(const char* aString);

  /**
   * Change the contents of this fragment to be a copy of the
   * the argument ucs2 string.
   */
  nsTextFragment& operator=(const PRUnichar* aString);

  /**
   * Change the contents of this fragment to be a copy of the
   * the argument string.
   */
  nsTextFragment& operator=(const nsAReadableString& aString);

  /**
   * Return PR_TRUE if this fragment is represented by PRUnichar data
   */
  PRBool Is2b() const {
    return mState.mIs2b;
  }

  /**
   * Get a pointer to constant PRUnichar data.
   */
  const PRUnichar* Get2b() const {
    NS_ASSERTION(Is2b(), "not 2b text"); 
    return m2b;
  }

  /**
   * Get a pointer to constant char data.
   */
  const char* Get1b() const {
    NS_ASSERTION(!Is2b(), "not 1b text"); 
    return (const char*) m1b;
  }

  /**
   * Get the length of the fragment. The length is the number of logical
   * characters, not the number of bytes to store the characters.
   */
  PRInt32 GetLength() const {
    return PRInt32(mState.mLength);
  }

  /**
   * Mutable version of Get2b. Only works for a non-const object.
   * Returns a pointer to the PRUnichar data.
   */
  PRUnichar* Get2b() {
    NS_ASSERTION(Is2b(), "not 2b text"); 
    return m2b;
  }

  /**
   * Mutable version of Get1b. Only works for a non-const object.
   * Returns a pointer to the char data.
   */
  char* Get1b() {
    NS_ASSERTION(!Is2b(), "not 1b text"); 
    return (char*) m1b;
  }

  /**
   * Change the contents of this fragment to be the given buffer and
   * length. The memory becomes owned by the fragment. In addition,
   * the memory for aBuffer must have been allocated using the 
   * nsIMemory interface.
   */
  void SetTo(PRUnichar* aBuffer, PRInt32 aLength, PRBool aRelease);

  /**
   * Change the contents of this fragment to be a copy of the given
   * buffer. Like operator= except a length is specified instead of
   * assuming 0 termination.
   */
  void SetTo(const PRUnichar* aBuffer, PRInt32 aLength) {
    ReleaseText();
    if (0 != aLength) {
      // See if we need to store the data in ucs2 or not
      PRBool need2 = PR_FALSE;
      const PRUnichar* ucp = aBuffer;
      const PRUnichar* uend = aBuffer + aLength;
      while (ucp < uend) {
        PRUnichar ch = *ucp++;
        if (ch >> 8) {
          need2 = PR_TRUE;
          break;
        }
      }

      if (need2) {
        // Use ucs2 storage because we have to
        PRUnichar* nt = (PRUnichar*)nsMemory::Alloc(aLength*sizeof(PRUnichar));
        if (nsnull != nt) {
          // Copy data
          nsCRT::memcpy(nt, aBuffer, sizeof(PRUnichar) * aLength);

          // Setup our fields
          m2b = nt;
          mState.mIs2b = 1;
          mState.mInHeap = 1;
          mState.mLength = aLength;
        }
      }
      else {
        // Use 1 byte storage because we can
        unsigned char* nt = (unsigned char*)nsMemory::Alloc(aLength*sizeof(unsigned char));
        if (nsnull != nt) {
          // Copy data
          unsigned char* cp = nt;
          unsigned char* end = nt + aLength;
          while (cp < end) {
            *cp++ = (unsigned char) *aBuffer++;
          }

          // Setup our fields
          m1b = nt;
          mState.mIs2b = 0;
          mState.mInHeap = 1;
          mState.mLength = aLength;
        }
      }
    }
  }

  /**
   * Change the contents of this fragment to be a copy of the given
   * buffer. Like operator= except a length is specified instead of
   * assuming 0 termination.
   */
  void SetTo(const char* aBuffer, PRInt32 aLength);

  /**
   * Append the contents of this string fragment to aString
   */
  void AppendTo(nsString& aString) const {
    if (mState.mIs2b) {
      aString.Append(m2b, mState.mLength);
    }
    else {
      aString.AppendWithConversion((char*)m1b, mState.mLength);
    }
  }

  /**
   * Make a copy of the fragments contents starting at offset for
   * count characters. The offset and count will be adjusted to
   * lie within the fragments data. The fragments data is converted if
   * necessary.
   */
  void CopyTo(PRUnichar* aDest, PRInt32 aOffset, PRInt32 aCount);

  /**
   * Make a copy of the fragments contents starting at offset for
   * count characters. The offset and count will be adjusted to
   * lie within the fragments data. The fragments data is converted if
   * necessary.
   */
  void CopyTo(char* aDest, PRInt32 aOffset, PRInt32 aCount);

  /**
   * Return the character in the text-fragment at the given
   * index. This always returns a PRUnichar.
   */
  PRUnichar CharAt(PRInt32 aIndex) const {
    NS_ASSERTION(PRUint32(aIndex) < mState.mLength, "bad index");
    return mState.mIs2b ? m2b[aIndex] : PRUnichar(m1b[aIndex]);
  }

protected:
  union {
    PRUnichar* m2b;
    unsigned char* m1b;
  };

public:
  struct FragmentBits {
    PRUint32 mInHeap : 1;
    PRUint32 mIs2b : 1;
    PRUint32 mLength : 30;
  };

protected:
  union {
    PRUint32 mAllBits;
    FragmentBits mState;
  };

  void ReleaseText() {
    if (mState.mLength && m1b && mState.mInHeap) {
      if (mState.mIs2b) {
        nsMemory::Free(m2b);
      }
      else {
        nsMemory::Free(m1b);
      }
    }
    m1b = nsnull;
    mState.mIs2b = 0;
    mState.mInHeap = 0;
    mState.mLength = 0;
  }
};

#endif /* nsTextFragment_h___ */

