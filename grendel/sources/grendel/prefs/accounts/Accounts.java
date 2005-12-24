/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Manager_Account.java
 *
 * Created on 10 August 2005, 12:10
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

package grendel.prefs.accounts;
import grendel.prefs.xml.XMLPreferences;
import java.io.File;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/**
 *
 * @author hash9
 */
public class Accounts extends XMLPreferences {
    
    /**
     * Creates a new instance of Accounts
     */
    public Accounts() {
    }
    
    /**
     * Creates a new instance of Accounts
     */
    public Accounts(XMLPreferences p) {
        super(p);
    }
    
    private int getNextEmptyAccountIndex() {
        Set<String> keys = this.keySet();
        boolean in_use = false;
        int i = -1;
        do {
            i++;
            in_use =  keys.contains(Integer.toString(i));
        } while(in_use);
        return i;
    }
    
    public void addAccount(Account a) {
        this.addAccount((AbstractAccount)a);
    }
    
    public void addAccount(AbstractAccount a) {
        this.setProperty(Integer.toString(getNextEmptyAccountIndex()),a);
    }
    
    public void removeAccount(int i) {
        this.remove(Integer.toString(i));
    }
    
    public Account getAccount(int i) {
        XMLPreferences retValue = this.getPropertyPrefs(Integer.toString(i));
        if (retValue == null) {
            return null;
        } else if (retValue instanceof Account) {
            return (Account) retValue;
        } else {
            return null;
        }
    }
    
    public List<Account__Receive> getReciveAccounts() {
        List<Account__Receive> l = new ArrayList<Account__Receive>(size());
        Collection c = values();
        for (Object o: c) {
            if (o instanceof Account__Receive) {
                l.add((Account__Receive) o);
            }
        }
        return l;
    }
    
    public List<Account__Send> getSendAccounts() {
        List<Account__Send> l = new ArrayList<Account__Send>(size());
        Collection c = values();
        for (Object o: c) {
            if (o instanceof Account__Send) {
                l.add((Account__Send) o);
            }
        }
        return l;
    }
    
    private static Map<String,Class> account_types = null;
    
    public static Map<String,Class> getAccountTypes() {
        if (account_types == null) {
            account_types = new TreeMap<String,Class>();
            try {
                String package_name = Accounts.class.getPackage().getName();
                
                // Get a File object for the package
                String s=Accounts.class.getResource(package_name.replace('.','/')).getFile();
                s=s.replace("%20", " ");
                
                File directory=new File(s);
                System.out.println("dir: "+directory.toString());
                
                if (directory.exists()) {
                    // Get the list of the files contained in the package
                    String[] files=directory.list();
                    
                    for (int i=0; i<files.length; i++) {
                        // we are only interested in .class files
                        if (files[i].endsWith(".class")) {
                            // removes the .class extension
                            String classname=files[i].substring(0, files[i].length()-6);
                            if ((!classname.startsWith("Account__")) && (classname.startsWith("Account_"))) {
                                
                                try {
                                    // Try to create an instance of the object
                                    Class c=Class.forName(package_name+"."+classname);
                                    if (AbstractAccount.class.isAssignableFrom(c)) {
                                        account_types.put(AbstractAccount.getType(c),c);
                                    }
                                } catch (ClassNotFoundException cnfex) {
                                    System.err.println(cnfex);
                                }
                            }
                        }
                    }
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return account_types;
    }
}
