/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

package grendel.composition;

import java.util.*;

import javax.mail.Address;

public class Addressee {
    private static final String ResourceFile = "addressee.res";

    public static final int TO          = 0;
    public static final int CC          = 1;
    public static final int BCC         = 2;
    public static final int GROUP       = 3;
    public static final int REPLY_TO    = 4;
    public static final int FOLLOWUP_TO = 5;

    public static final String[] mDeliveryStr = {"To:", "Cc:", "Bcc:", "Group:", "Reply-To:", "Followup-To:"};
//    public static final String[] mDeliveryStr;

    private String  mAddress;
    private int     mDelivery;
    private ResourceBundle resources;
/*
    static {
        String[] mDeliveryName = {
                    "address_to",
                    "address_cc",
                    "address_bcc",
                    "address_group",
                    "address_reply_to",
                    "address_followup_to"};

        mDeliveryStr = new String[mDeliveryName.length];
        ResourceBundle resources = ResourceBundle.getBundle(ResourceFile, Locale.getDefault());

        for (int i = 0; i < mDeliveryName.length; i++) {
            String str;

            try {
                str = resources.getString(mDeliveryName[i]);
            }
            catch (MissingResourceException mre) {
                str = "";
            }

            mDeliveryStr[i] = str;
        }
    }
*/
    public Addressee (String aAddress, int aDelivery) {
        init (aAddress, aDelivery);
    }

    public Addressee () {
        init ("", TO);
    }

    public Addressee(Address addr, int delivery) {
        init(addr.toString(), delivery);
    }


    public void init (String aAddress, int aDelivery) {
        setText (aAddress);
        setDelivery (aDelivery);
    }

    public String   getText () { return mAddress; }
    public void     setText (String aAddress) { mAddress = aAddress; }

    public int      getDelivery () { return mDelivery; }
    public void     setDelivery (int aDelivery) { mDelivery = aDelivery; }

    public String deliveryString () { return deliveryToString(mDelivery); };

    public static String getLongestString() {
        return "Followup-To:";
    }

    public static String getDeliveryTitle() {
        return "Delivery";
    }

    public static String deliveryToString (int aDelivery) {
        if ((aDelivery > -1) && (aDelivery < mDeliveryStr.length))
            return mDeliveryStr [aDelivery];

        return mDeliveryStr [TO];
    }

    protected static int deliveryToInt(String deliverString) {
        for (int i = 0; i < mDeliveryStr.length; i++) {
            if (deliverString.equalsIgnoreCase (mDeliveryStr[i]))
                return (i);
        }

        return (TO);    //default
    }
}
