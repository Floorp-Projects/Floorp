/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
 Client QA Team, St. Petersburg, Russia
*/


import java.util.Stack;
import java.util.EmptyStackException;

public class VarContainer {
    //Compatibility section
    public static byte byteVar = -123;
    public static short shortVar  = -30001;
    public static int intVar = -2147483601;   
    public static long longVar = -93674872;
    public static byte unsignedByteVar = 101;
    public static short unsignedShortVar = 601;
    public static int unsignedIntVar = 3000000;
    public static long unsignedLongVar = 1000001;
    public static float floatVar = (float)2.0;
    public static double doubleVar = 3.0;
    public static boolean booleanVar = true;
    public static char charTruncVar = 'A';
    public static char charVar = 'B';
    public static String charPVar = "This is some string";
    public static String unicharPVar = "UUUAAARRR";
    //end of compatibility section
    
    public static byte endOfData = 112;

	public static  byte byteMin = 0;
	public static  byte byteMid = 1;
	public static  byte byteMax = 127;
	public static  short shortMin = Short.MIN_VALUE;
	public static  short shortZerro = 0;
	public static  short shortMid = 1;
	public static  short shortMax = Short.MAX_VALUE;
	public static  int intMin = Integer.MIN_VALUE;
	public static  int intZerro = 0;
	public static  int intMid = 1;
	public static  int intMax = Integer.MAX_VALUE;
	public static  long longMin = Long.MIN_VALUE;
	public static  long longZerro = 0;
	public static  long longMid = 1;
	public static  long longMax = Long.MAX_VALUE;
	public static  short ushortMin = 0;
	public static  short ushortMid = 1;
	public static  short ushortMax = Short.MAX_VALUE;
	public static  int uintMin = 0;
	public static  int uintMid = 1;
	public static  int uintMax = Integer.MAX_VALUE;
	public static  long ulongMin = 0;
	public static  long ulongMid = 1;
	public static  long ulongMax = Long.MAX_VALUE;
	public static  float floatMin = Float.MIN_VALUE;
	public static  float floatZerro = 0;
	public static  float floatMid = 1;
	public static  float floatMax = Float.MAX_VALUE;
	public static  double doubleMin = Double.MIN_VALUE;
	public static  double doubleZerro = 0;
	public static  double doubleMid = 1;
	public static  double doubleMax = Double.MAX_VALUE;
	public static  char charFirst = '0';
	public static  char charLast = 'Z';
	public static  char wcharFirst = '0';
	public static  char wcharLast = 'Z';
	public static  String stringFirst = "iddqd";
	public static  String stringEmpty = "";
	public static  String stringNull = "Here must be the NULL string";
	public static  String stringLast = "112";

    //stacks
public Stack byteStack = null;
public Stack shortStack = null;
public Stack intStack = null;
public Stack longStack = null;
public Stack ushortStack = null;
public Stack uintStack = null;
public Stack ulongStack = null;
public Stack floatStack = null;
public Stack doubleStack = null;
public Stack charStack = null;
public Stack wcharStack = null;
public Stack stringStack = null;
    //Constructor with stacks initialization
public VarContainer() {
    byteStack = new Stack();
    byteStack.push(new Byte(byteMin));
    byteStack.push(new Byte(byteMid));
    byteStack.push(new Byte(byteMax));
	shortStack = new Stack();
    shortStack.push(new Short(shortMin));
    shortStack.push(new Short(shortMid));
    shortStack.push(new Short(shortMax));
    shortStack.push(new Short(shortZerro));
	intStack = new Stack();
    intStack.push(new Integer(intMin));
    intStack.push(new Integer(intMid));
    intStack.push(new Integer(intMax));
    intStack.push(new Integer(intZerro));
	longStack = new Stack();
    longStack.push(new Long(longMin));
    longStack.push(new Long(longMid));
    longStack.push(new Long(longMax));
    longStack.push(new Long(longZerro));
	ushortStack = new Stack();
    ushortStack.push(new Short(ushortMin));
    ushortStack.push(new Short(ushortMid));
    ushortStack.push(new Short(ushortMax));
	uintStack = new Stack();
    uintStack.push(new Integer(uintMin));
    uintStack.push(new Integer(uintMid));
    uintStack.push(new Integer(uintMax));
	ulongStack = new Stack();
    ulongStack.push(new Long(ulongMin));
    ulongStack.push(new Long(ulongMid));
    ulongStack.push(new Long(ulongMax));
	floatStack = new Stack();
    floatStack.push(new Float(floatMin));
    floatStack.push(new Float(floatMid));
    floatStack.push(new Float(floatMax));
    floatStack.push(new Float(floatZerro));
	doubleStack = new Stack();
    doubleStack.push(new Double(doubleMin));
    doubleStack.push(new Double(doubleMid));
    doubleStack.push(new Double(doubleMax));
    doubleStack.push(new Double(doubleZerro));
	charStack = new Stack();
    charStack.push(new Character(charFirst));
    charStack.push(new Character(charLast));
    wcharStack = new Stack();
    wcharStack.push(new Character(wcharFirst));
    wcharStack.push(new Character(wcharLast));
	stringStack = new Stack();
    stringStack.push(stringFirst);
    stringStack.push(stringEmpty);
    stringStack.push(stringNull);
    stringStack.push(stringLast);

}

public byte getNextByte() {
    try {
        return ((Byte)byteStack.pop()).byteValue();
    }catch(EmptyStackException e) {
        return endOfData;
    }
}
public short getNextShort() {
    try {
        return ((Short)shortStack.pop()).shortValue();
    }catch(EmptyStackException e) {
        return endOfData;
    }
}


public int getNextInt() {
    try {
        return ((Integer)intStack.pop()).intValue();
    }catch(EmptyStackException e) {
        return endOfData;
    }
}
public long getNextLong() {
    try {
        return ((Long)longStack.pop()).longValue();
    }catch(EmptyStackException e) {
        return endOfData;
    }
}
public short getNextUshort() {
    try {
        return ((Short)ushortStack.pop()).shortValue();
    }catch(EmptyStackException e) {
        return endOfData;
    }
}

public int getNextUint() {
    try {
        return ((Integer)uintStack.pop()).intValue();
    }catch(EmptyStackException e) {
        return endOfData;
    }
}
public long getNextUlong() {
    try {
        return ((Long)ulongStack.pop()).longValue();
    }catch(EmptyStackException e) {
        return endOfData;
    }
}

public float getNextFloat() {
    try {
        return ((Float)floatStack.pop()).floatValue();
    }catch(EmptyStackException e) {
        return endOfData;
    }
}
public double getNextDouble() {
    try {
        return ((Double)doubleStack.pop()).doubleValue();
    }catch(EmptyStackException e) {
        return endOfData;
    }
}

public char getNextChar() {
    try {
        return ((Character)charStack.pop()).charValue();
    }catch(EmptyStackException e) {
        return (char)endOfData;
    }
}
public char getNextWchar() {
    try {
        return ((Character)wcharStack.pop()).charValue();
    }catch(EmptyStackException e) {
        return (char)endOfData;
    }
}

public String getNextString() {
    try {
        return (String)stringStack.pop();
    }catch(EmptyStackException e) {
        return stringLast;
    }
}

}


