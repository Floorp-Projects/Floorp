/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
package com.netscape.sasl;

/**
 * This class provides a SASLClientMechanismDriver, or a list of mechanisms.
 */
public class SASLMechanismFactory {

    /**
     * Returns an object implementing a requested mechanism driver. A
     * SASLException is thrown if no corresponding mechanism driver can be
     * instantiated.
     * @param mechanisms A list of mechanism names
     * @param packageName A package from which to instantiate the mechanism
     *    driver, eg, "myclasses.SASL.mechanisms". If null, a system default
     *    is used.
     */
    public static SASLClientMechanismDriver getMechanismDriver(
      String[] mechanisms, String packageName) throws SASLException {

        for (int i=0; i<mechanisms.length; i++) {
            try {
                SASLClientMechanismDriver driver =
                  getMechanismDriver(mechanisms[i], packageName);
                if (driver != null)
                    return driver;
            } catch (SASLException e) {
            }
        }

        throw new SASLException();
    }

    public static SASLClientMechanismDriver getMechanismDriver(
      String mechanism, String packageName) throws SASLException {

        String className = packageName+"."+mechanism;
        SASLClientMechanismDriver driver = null;

        try {
            Class c = Class.forName(className);
            java.lang.reflect.Constructor[] m = c.getConstructors();
            for (int i = 0; i < m.length; i++) {
                /* Check if the signature is right: String */
                Class[] params = m[i].getParameterTypes();
                if (params.length == 0) {
                    driver = 
                      (SASLClientMechanismDriver)(m[i].newInstance(null));
                    return driver;
                } else if ((params.length == 1) &&
                  (params[0].getName().equals("java.lang.String"))) {
                    Object[] args = new Object[1];
                    args[0] = mechanism;
                    driver =
                      (SASLClientMechanismDriver)(m[i].newInstance(args));
                    return driver;
                }
            }
            System.out.println("No appropriate constructor in " + mechanism);
        } catch (ClassNotFoundException e) {
            System.out.println("Class "+mechanism+" not found");
        } catch (Exception e) {
            System.out.println("Failed to create "+mechanism+
              " mechanism driver");
        }

        throw new SASLException();
    }

    public static String[] getMechanisms() throws SASLException {
        throw new SASLException("Method getMechanisms not supported now", 80);
    }

    public static String[] getMechanisms(String packageName)
      throws SASLException {
        throw new SASLException("Method getMechanisms not supported now");
    }
}
