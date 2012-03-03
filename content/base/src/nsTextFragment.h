/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * A class which represents a fragment of text (eg inside a text
 * node); if only codepoints below 256 are used, the text is stored as
 * a char*; otherwise the text is stored as a PRUnichar*
 */

#ifndef nsTextFragment_h___
#define nsTextFragment_h___

#include "mozilla/Attributes.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsTraceRefcnt.h"
#include "nsDOMMemoryReporter.h"

class nsString;
class nsCString;

// XXX should this normalize the code to keep a \u0000 at the end?

// XXX nsTextFragmentPool?

// XXX these need I18N spankage
#define XP_IS_SPACE(_ch) \
  (((_ch) == ' ') || ((_ch) == '\t') || ((_ch) == '\n') || ((_ch) == '\r'))

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
class nsTextFragment MOZ_FINAL {
public:
  static nsresult Init();
  static void Shutdown();

  /**
   * Default constructor. Initialize the fragment to be empty.
   */
  nsTextFragment()
    : m1b(nsnull), mAllBits(0)
  {
    MOZ_COUNT_CTOR(nsTextFragment);
    NS_ASSERTION(sizeof(FragmentBits) == 4, "Bad field packing!");
  }

  ~nsTextFragment();

  /**
   * Change the contents of this fragment to be a copy of the
   * the argument fragment.
   */
  nsTextFragment& operator=(const nsTextFragment& aOther);

  /**
   * Return true if this fragment is represented by PRUnichar data
   */
  bool Is2b() const
  {
    return mState.mIs2b;
  }

  /**
   * Return true if this fragment contains Bidi text
   * For performance reasons this flag is only set if explicitely requested (by
   * setting the aUpdateBidi argument on SetTo or Append to true).
   */
  bool IsBidi() const
  {
    return mState.mIsBidi;
  }

  /**
   * Get a pointer to constant PRUnichar data.
   */
  const PRUnichar *Get2b() const
  {
    NS_ASSERTION(Is2b(), "not 2b text"); 
    return m2b;
  }

  /**
   * Get a pointer to constant char data.
   */
  const char *Get1b() const
  {
    NS_ASSERTION(!Is2b(), "not 1b text"); 
    return (const char *)m1b;
  }

  /**
   * Get the length of the fragment. The length is the number of logical
   * characters, not the number of bytes to store the characters.
   */
  PRUint32 GetLength() const
  {
    return mState.mLength;
  }

  bool CanGrowBy(size_t n) const
  {
    return n < (1 << 29) && mState.mLength + n < (1 << 29);
  }

  /**
   * Change the contents of this fragment to be a copy of the given
   * buffer. If aUpdateBidi is true, contents of the fragment will be scanned,
   * and mState.mIsBidi will be turned on if it includes any Bidi characters.
   */
  void SetTo(const PRUnichar* aBuffer, PRInt32 aLength, bool aUpdateBidi);

  /**
   * Append aData to the end of this fragment. If aUpdateBidi is true, contents
   * of the fragment will be scanned, and mState.mIsBidi will be turned on if
   * it includes any Bidi characters.
   */
  void Append(const PRUnichar* aBuffer, PRUint32 aLength, bool aUpdateBidi);

  /**
   * Append the contents of this string fragment to aString
   */
  void AppendTo(nsAString& aString) const {
    if (mState.mIs2b) {
      aString.Append(m2b, mState.mLength);
    } else {
      AppendASCIItoUTF16(Substring(m1b, mState.mLength), aString);
    }
  }

  /**
   * Append a substring of the contents of this string fragment to aString.
   * @param aOffset where to start the substring in this text fragment
   * @param aLength the length of the substring
   */
  void AppendTo(nsAString& aString, PRInt32 aOffset, PRInt32 aLength) const {
    if (mState.mIs2b) {
      aString.Append(m2b + aOffset, aLength);
    } else {
      AppendASCIItoUTF16(Substring(m1b + aOffset, aLength), aString);
    }
  }

  /**
   * Make a copy of the fragments contents starting at offset for
   * count characters. The offset and count will be adjusted to
   * lie within the fragments data. The fragments data is converted if
   * necessary.
   */
  void CopyTo(PRUnichar *aDest, PRInt32 aOffset, PRInt32 aCount);

  /**
   * Return the character in the text-fragment at the given
   * index. This always returns a PRUnichar.
   */
  PRUnichar CharAt(PRInt32 aIndex) const
  {
    NS_ASSERTION(PRUint32(aIndex) < mState.mLength, "bad index");
    return mState.mIs2b ? m2b[aIndex] : static_cast<unsigned char>(m1b[aIndex]);
  }

  struct FragmentBits {
    // PRUint32 to ensure that the values are unsigned, because we
    // want 0/1, not 0/-1!
    // Making these bool causes Windows to not actually pack them,
    // which causes crashes because we assume this structure is no more than
    // 32 bits!
    PRUint32 mInHeap : 1;
    PRUint32 mIs2b : 1;
    PRUint32 mIsBidi : 1;
    PRUint32 mLength : 29;
  };

  size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

private:
  void ReleaseText();

  /**
   * Scan the contents of the fragment and turn on mState.mIsBidi if it
   * includes any Bidi characters.
   */
  void UpdateBidiFlag(const PRUnichar* aBuffer, PRUint32 aLength);
 
  union {
    PRUnichar *m2b;
    const char *m1b; // This is const since it can point to shared data
  };

  union {
    PRUint32 mAllBits;
    FragmentBits mState;
  };
};

#endif /* nsTextFragment_h___ */

