/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

package calypso.util;

import java.util.logging.*;

/** A class for counting in a variety of bases.
 */

public class Abacus
{
  private static final Logger logger = Logger.getLogger("calypso.util.Abacus");

/************************************************
 *    Formatting Strings
 ************************************************/

  public final static String ones[]=
    {"zero","one ","two ","three ","four ","five ",
     "six ","seven ","eight ","nine ","ten "};

  public final static String teens[]=
    {"ten ","eleven ","twelve ","thirteen ","fourteen ","fifteen ",
     "sixteen ","seventeen ","eighteen ","nineteen "};

  public final static String tens[]=
    {"","ten ","twenty ","thirty ","forty ","fifty ",
     "sixty ","seventy ","eighty ","ninety ","hundred "};

  public final static String bases[] =
    {"","hundred ","thousand ","million ","billion ","trillion ","quadrillion "};

  protected final static String gAlphaChars  = "abcdefghijklmnopqrstuvwxyz";
  protected final static String gRomanCharsA = "ixcm";
  protected final static String gRomanCharsB = "vld?";
  protected final static String gHexChars    = "0123456789abcdef";
  protected final static String gBinaryChars = "01";

/************************************************
 *    Data members...
 ************************************************/

      //notice that we don't have any?

  /** Formats the value as an alpha string
   *
   *
   * @return
   * @author  gess   05-05-97 12:22pm
   * @notes
   */
  public static String getAlphaString(int aValue)
  {
    return getSeriesString(Math.abs(aValue)-1,gAlphaChars,-1,gAlphaChars.length());
  }

  /**
   * Convert the given integer value into a roman numeral string
   *
   * @param   aValue - int to be converted to roman
   * @return  new string
   * @author  gess   05-05-97 12:53pm
   * @notes   Those pesky romans used dashed above a value to multiply
   *          that number by 1000. This has the unfortunate side effect
   *          that without special font treatment, we cant represent
   *          numbers in roman above 3999.
   *          THIS METHOD HANDLES VALUES IN THE RANGE [1..3999].
   *          (any other value is converted to simple numeric format.)
   */
  public static String getRomanString(int aValue)
  {
    StringBuffer  addOn  = new StringBuffer();
    StringBuffer  result = new StringBuffer();
    StringBuffer  decStr = new StringBuffer();

    decStr.append(aValue);

    int           len=decStr.length();
    int           romanPos=len;
    int           n,digitPos;
    boolean       negative=(aValue<0);

    aValue=Math.abs(aValue);
    for(digitPos=0;digitPos<len;digitPos++)
    {
      romanPos--;
      addOn.setLength(0);
      switch(decStr.charAt(digitPos))
      {
        case  '3':  addOn.append(gRomanCharsA.charAt(romanPos));
        case  '2':  addOn.append(gRomanCharsA.charAt(romanPos));
        case  '1':  addOn.append(gRomanCharsA.charAt(romanPos));
          break;

        case  '4':
          addOn.append(gRomanCharsA.charAt(romanPos));

        case  '5':  case  '6':
        case  '7':  case  '8':
          addOn.append(gRomanCharsB.charAt(romanPos));
          for(n=0;n<(decStr.charAt(digitPos)-'5');n++)
            addOn.append(gRomanCharsA.charAt(romanPos));
          break;
        case  '9':
          addOn.append(gRomanCharsA.charAt(romanPos));
          addOn.append(gRomanCharsA.charAt(romanPos+1));
          break;
        default:
          break;
      }
      result.append(addOn);
    }
    return result.toString();
  }

  /**
   * Convert the given integer value into a hexstring
   *
   * @param   aValue - int to be converted to hex string
   * @return  new string
   * @author  gess   05-05-97 12:53pm
   *
   */
  public static String getHexString(int aValue)
  {
    if (aValue<0)
      aValue=65536-Math.abs(aValue);
    return getSeriesString(aValue,gHexChars,0,gHexChars.length());
  }

  /**
   * Convert the given integer value into a string of binary digits
   *
   * @param   aValue - int to be converted to binary string
   * @return  new string
   * @author  gess   05-05-97 12:53pm
   *
   */
  public static String getBinaryString(int aValue)
  {
    if (aValue<0)
      aValue=65536-Math.abs(aValue);
    return getSeriesString(aValue,gBinaryChars,0,gBinaryChars.length());
  }

  /**
   * Convert the given integer value into spoken string (one, two, three...)
   *
   * @param   aValue - int to be converted to hex string
   * @return  new stringbuffer
   * @author  gess   05-05-97 12:53pm
   *
   */
  public static String getSpokenString(int aValue)
  {
    int           root=1000000000;
    int           expn=4;
    int           modu=0;
    int           div=0;
    int           temp=0;
    StringBuffer  result= new StringBuffer();

    if (aValue<0)
      result.append('-');

    aValue=Math.abs(aValue);
    while((0!=root) && (0!=aValue))
    {
      temp=aValue/root;
      if(0!=temp)
      {
        div=temp/100;
        if (0!=div)             //start with hundreds part
        {
          result.append(ones[div]);
          result.append(bases[1]);
        }
        modu=(temp%10);
        div=((temp%100)/10);

        if (0!=div)
          if (div<2)
          {
            result.append(teens[modu]);
            modu=0;
          }
          else result.append(tens[div]);
        if (0!=modu)
          result.append(ones[modu]); // do remainder.
        aValue-=(temp*root);
        if (expn>1) result.append(bases[expn]);
      }
      expn--;
      root/=1000;
    }
    if (0==result.length())
      result.append("zero");
    return result.toString();
  }

  /**
   * Convert the given integer value into a series string. These are any
   * arbitrary but repeating pattern of characters.
   *
   * @param   aValue - int to be converted to series string
   * @return  new string
   * @author  gess   05-05-97 12:53pm
   * @notes   this method gets used for binary and hex conversion
   */
  public static String getSeriesString(int aValue,String aCharSet,int anOffset,int aBase)
  {
    int ndex=0;
    int root=1;
    int next=aBase;
    int expn=1;
    StringBuffer result=new StringBuffer();

    aValue=Math.abs(aValue);  // must be positive here...
    while(next<=aValue)       // scale up in baseN; exceed current value.
    {
      root=next;
      next*=aBase;
      expn++;
    }

    while(0!=(expn--))
    {
      ndex = ((root<=aValue) && (0!=root)) ? (aValue/root): 0;
      aValue%=root;
      if(root>1)
        result.append(aCharSet.charAt(ndex+(1*anOffset)));
      else result.append(aCharSet.charAt(ndex));
      root/=aBase;
    };
    return result.toString();
  }

  public static void PadPrint(int aValue,int width)
  {
    StringBuffer temp=new StringBuffer();
    temp.append(aValue);
    PadPrint(temp.toString(),width);
  }

  public static void PadPrint(String aString, int aWidth)
  {
    logger.info(aString);
    int padCount=aWidth-aString.length();
    if(padCount>0)
      for(int i=0;i<padCount;i++)
        logger.info(" ");
  }


  public static void main(String argv[])
  {
    int index=0;

    logger.info(" \nValue   Hex       Roman     Series    Binary            Spoken    \n-----------------------------------------------------------------------------");
    for(index=1002;index<1205;index++)
    {
      PadPrint(index,8);
      PadPrint(Abacus.getHexString(index),10);
      PadPrint(Abacus.getRomanString(index),10);
      PadPrint(Abacus.getSeriesString(index,"ABCD",0,4),10);
      PadPrint(Abacus.getBinaryString(index),16);
      PadPrint(Abacus.getSpokenString(index),10);
      logger.info("");
    }
  }

}

