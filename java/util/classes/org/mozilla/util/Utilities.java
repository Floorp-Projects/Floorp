/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is "The Lighthouse Foundation Classes (LFC)"
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1997, 1998, 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


package org.mozilla.util;

import java.util.Date;
import java.util.Enumeration;
import java.util.Vector;
import java.util.StringTokenizer;
import java.util.ResourceBundle;
import java.util.MissingResourceException;

import java.awt.Component;
import java.awt.Container;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.UnsupportedEncodingException;

public class Utilities extends Object {
    // PENDING(kbern) NOTE: These Vector methods should eventually move into a file
    // called "VectorUtilities", must like the already-existing "HashtableUtilities".
    /**
     * Take the given string and chop it up into a series
     * of strings on "delimiter" boundries.  This is useful
     * for trying to get an array of strings out of the
     * resource file.
     */
    static public Vector vectorFromString(String input, String delimiter) {
	Vector aVector = new Vector();
	StringTokenizer aTokenizer = new StringTokenizer(input, delimiter);

	while (aTokenizer.hasMoreTokens())
	    aVector.addElement(aTokenizer.nextToken());
	return aVector;
    }
    
    /**
    * Creates a String by combining the elements of aVector.
    * after each element it will insert the String "delimiter".
    * If no "delimiter" is desired, the "delimiter" parameter should be ""
    */
    static public String stringFromVector(Vector aVector, String delimiter) {
	String returnString = null;

	if (aVector != null) {
	    Enumeration vectorEnumerator = aVector.elements();
	    
	    if (delimiter == null) {
		delimiter = ""; // Might as well be nice to sloppy caller!
	    }
	    while (vectorEnumerator.hasMoreElements()) {
		if (returnString == null) {
		    returnString = "";
		} else {
		    returnString += delimiter;
		}
		returnString += vectorEnumerator.nextElement();
	    }
	}
	return (returnString == null) ? "" : returnString;
    }

    /**
    * Convert an Array into a Vector. Can you *believe* that there is no Vector constructor
    * which takes an Array!!! &()*&$#(*&$
    */
    static public Vector vectorFromArray(Object[] anArray) {
	Vector returnVector;
	
	if ((anArray != null) && (anArray.length > 0)) {
	    returnVector = new Vector(anArray.length);
	    for (int i = anArray.length - 1; i >= 0; i--) {
		returnVector.addElement(anArray[i]);
	    }
	} else {
	    returnVector = new Vector(0);
	}
	return returnVector;
    }
    
    /**
    * Amazing that "Vector" does not override Object's "equals()" method to do this
    * itself!
    */
    static public boolean vectorsAreEqual(Vector vectorOne, Vector vectorTwo) {
	boolean returnValue = vectorOne.equals(vectorTwo); // Give "Object" a chance.
	
	if (!returnValue && (vectorOne != null) && (vectorTwo != null)) {
	    int vectorSize = vectorOne.size();
	    
	    if (vectorSize == vectorTwo.size()) {
		int index;
		
		// "Object" says, "No", but maybe they really are... let's do the long check...
		for (index = 0; index < vectorSize; index++) {
		    if (!(vectorOne.elementAt(index).equals(vectorTwo.elementAt(index)))) {
			break;
		    }
		}
		if (index == vectorSize) {
		    // We made it to the end without "break"ing! They must be equal...
		    returnValue = true;
		}
	    } // else, they're definately not equal!
	}
	return returnValue;
    }
    
    /**
    * Returns "true" if the passed in array contains the passed in element.
    * Checks for equality using ".equals()".
    * Returns "false" if "anArray" is null.
    */
    static public boolean arrayContainsElement(Object[] anArray, Object anElement) {
	boolean returnValue = false;
	
	if (anArray != null) {
	    for (int index = 0; (!returnValue && (index < anArray.length)); index++) {
		if (anArray[index].equals(anElement)) {
		    returnValue = true;
		}
	    }
	}
	return returnValue;
    }
    
    /**
    * Removes leading, trailing, and internal whitespace from the passed-in
    * string.
    * Returns a new string without any whitespace.   
    */
    static public String removeAllWhitespace(String aString) {
	String returnString = aString;
	
	if (aString != null) {
	    returnString = aString.trim();
	    Vector stringComponents = Utilities.vectorFromString(aString, " ");
	    
	    returnString = Utilities.stringFromVector(stringComponents, "");
	}
	return returnString;
    }

    /**
    * Can return a string of the form "5:35:09pm", as opposed to "17:35:09"
    * If "useTwentyFourHourTime" is "true", returns time in the form "17:35:09"
    * If "showAMPMIndicator" is "true" it will include the "am" or "pm" text,
    * otherwise it won't.
    * Note that the "showAMPMIndicator" field is ignored if "useTwentyFourHourTime" is "true"
    * since it provides redundant information in that case.
    */
    static public String currentTimeString(boolean useTwentyFourHourTime, boolean showAMPMIndicator) {
	// "ugly" is of the form, "17:35:09"
	String returnString = new Date().toString().substring(11, 19);
	
	if (!useTwentyFourHourTime) {
	    Vector dateComponents = Utilities.vectorFromString(returnString, ":");
	    try {
		String ampmString;
		int hourField = Integer.parseInt((String)(dateComponents.elementAt(0)));
		
		// Make it on 12 hour clock, with am and pm...
		if (hourField > 12) {
		    // More common than == 12
		    ampmString = "pm";
		    hourField -= 12;
		    dateComponents.setElementAt(Integer.toString(hourField), 0);
		    returnString = Utilities.stringFromVector(dateComponents, ":");
		} else if (hourField == 12) {
		    ampmString = "pm";
		} else {
		    ampmString = "am";
		}
		if (showAMPMIndicator) {
		    returnString += ampmString;
		}
	    } catch (NumberFormatException anException) {
	    }	    
	}
	return returnString;
    }

    /**
    * Uses "getParent()" to find this Component's top-level ancestor.
    *
    * If this Component has no ancestors, this method will return the Component itself.
    */
    static public Component getTopLevelParent(Component aComponent) {
	Component returnComponent = aComponent;
	
	if (aComponent != null) {
	    Component testComponent = aComponent;
	    
	    do {
		if (testComponent instanceof Container) {
		    returnComponent = testComponent;
		    testComponent = ((Container)testComponent).getParent();
		} else {
		    testComponent = null;
		}
	    } while (testComponent != null);
	}
	return returnComponent;
    }

    /**
     * @param aClass the class whose package name should be returned
     *
     * @return the fully qualified package name of the given class, null
     * if not found
     */

    static public String getPackageName(Class aClass) {
	if (null == aClass) {
	    return null;
	}
	String baseName = aClass.getName();
	int index = baseName.lastIndexOf('.');
	return (index < 0 ? "" : baseName.substring(0, index+1));
    }

    /**
     *

     * This method is a simpler alternative to
     * ResourceLoader.loadResourceBundle.  Instead of returning an
     * PropertyResourceBundle, as ResourceLoader.loadResourceBundle
     * does, it must returns a java.util.ResourceBundle

     *
     * @param baseName the fully qualified name of the resource bundle,
     * sans "<CODE>.properties</CODE>" suffix.  For example, a
     * valid value for baseName would be
     * <CODE>com.sun.jag.apps.spex.util.SUResources</CODE> when the
     * properties file <CODE>SUResources.properties</CODE> is in the
     * classpath under the directory
     * <CODE>com/sun/jag/apps/spex/util</CODE>.
     *
     * @return the actual ResourceBundle instance, or null if not found.  

     * <!-- see ResourceLoader#loadResourceBundle -->

     */

    static public ResourceBundle getResourceBundle(String baseName) {
	ResourceBundle resourceBundle = null;
	try {
	    resourceBundle = ResourceBundle.getBundle(baseName);
	}
	catch (MissingResourceException e) {
	    Log.logError("Missing resource bundle: " + baseName);
	}
	return resourceBundle;
    }

    /**
    Case insensitive String.endsWith()
     */
    public static boolean endsWithIgnoringCase(String aString, String possibleEnding) {
        int endingLength;
        if (aString == null || possibleEnding == null)
            return false;
        endingLength = possibleEnding.length();
        if (aString.length() < endingLength)
            return false;
        return aString.regionMatches(true, aString.length() - endingLength,
                                     possibleEnding, 0, endingLength);
    }

    /**
     *
     * <p>This method tries to load the resource
     * <code>META-INF/services/&gt;interfaceClassName&gt;</code>, where
     * <code>&gt;interfaceClassName&lt;</code> is the argument to this
     * method.  If the resource is found, interpret it as a
     * <code>Properties</code> file and read out its first line.
     * Interpret the first line as the fully qualified class name of a
     * class that implements <code>interfaceClassName</code>.  The named
     * class must have a public no-arg constructor.</p>
     *
     */


    public static Object getImplFromServices(String interfaceClassName) {
        Object instance = null;
        
        ClassLoader classLoader = 
            Thread.currentThread().getContextClassLoader();
        if (classLoader == null) {
            throw new RuntimeException("Context ClassLoader");
        }
        
        BufferedReader reader = null;
        InputStream stream = null;
        String 
            className = null,
            resourceName = "META-INF/services/" + interfaceClassName;
        try {
            stream = classLoader.getResourceAsStream(resourceName);
            if (stream != null) {
                // Deal with systems whose native encoding is possibly
                // different from the way that the services entry was created
                try {
                    reader =
                        new BufferedReader(new InputStreamReader(stream,
                                                                 "UTF-8"));
                } catch (UnsupportedEncodingException e) {
                    reader = new BufferedReader(new InputStreamReader(stream));
                }
                className = reader.readLine();
                reader.close();
                reader = null;
                stream = null;
            }
        } catch (IOException e) {
        } catch (SecurityException e) {
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (Throwable t) {
                    ;
                }
                reader = null;
                stream = null;
            }
            if (stream != null) {
                try {
                    stream.close();
                } catch (Throwable t) {
                    ;
                }
                stream = null;
            }
        }
        if (null != className) {
            try {
                Class clazz = classLoader.loadClass(className);
                instance = clazz.newInstance();
            } catch (Exception e) {
            }
        }
        return instance;
    }
}

