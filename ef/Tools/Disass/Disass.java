/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
public class Disass
{
	final static int SUCCESS = 0;
	final static int UNKNOWN_STUB = 1;
	final static int METHOD_NOT_FOUND = 2;
	final static int CLASS_NOT_FOUND = 3;

	public native int disassemble(String className, String methodName, String signature);

	Disass()
	{
		System.loadLibrary("Disass");
	}

	public static void main(String argv[])
	{
		if (argv.length != 3) {
			System.out.println("usage: Disass <ClassName> <MethodName> <Signature>");
			return;
		}

		Disass disass = new Disass();
 
		switch (disass.disassemble(argv[0], argv[1], argv[2])) {
			case SUCCESS:
				break;

			case UNKNOWN_STUB:
				System.out.println("Was unable to compile the method (unknown compile stub)");
				break;

			case METHOD_NOT_FOUND:
				System.out.println("Found the class " + argv[0] + " but cannot find the method "
								    + argv[1] + argv[2] + ".");
				break;

			case CLASS_NOT_FOUND:
				System.out.println("Cannot find the class " + argv[0] + ".");
				break;
		}
	}

	public void test(int a)
	{
		int j = 0;

		for (int i = 0; i < 100; i++) {
			j += i;
		}
	}
}
