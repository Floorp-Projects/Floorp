/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 *  except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is MozillaTranslator (Mozilla Localization Tool)
 *
 * The Initial Developer of the Original Code is Henrik Lynggaard Hansen
 *
 * Portions created by Henrik Lynggard Hansen are
 * Copyright (C) Henrik Lynggaard Hansen.
 * All Rights Reserved.
 *
 * Contributor(s):
 * Henrik Lynggaard Hansen (Initial Code)
 *
 */
package org.mozilla.translator.datamodel;

import java.io.*;
import java.util.*;
import javax.swing.*;

import org.mozilla.translator.io.*;


/**
 *
 * @author  Henrik Lynggaard Hansen
 * @version 4.0
 */
public class MozFile extends MozTreeNode
{
    
    public static final int TYPE_DTD=1;
    public static final int TYPE_PROP=2;
    public static final int TYPE_RDF=3;
    public static final int TYPE_UNSUPPORTED=4;
    
    private int type;
    
  /** Creates new MozFile */
    public MozFile(String n,MozTreeNode p)
    {

        super(n,p);
        if (n.endsWith(".dtd"))
        {
            type=TYPE_DTD;
        }
        else if (n.endsWith(".properties"))
        {
            type=TYPE_PROP;
        }
        else
        {
            type=TYPE_UNSUPPORTED;
        }

    }
    
    public int getType()
    {
        return type;
    }
    
    public void setName(String fileName)
    {
        this.name=fileName;
        if (fileName.endsWith(".dtd"))
        {
            type=TYPE_DTD;
        }
        else if (fileName.endsWith(".properties"))
        {
            type=TYPE_PROP;
        }
        else
        {
            type=TYPE_UNSUPPORTED;
        }
    }
   
}