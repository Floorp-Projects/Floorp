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
package org.mozilla.translator.gui.models;

import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.kernel.*;
/**
 *
 * @author  Henrik Lynggaard Hansen
 * @version 4.0
 */
public class ComplexColumn extends Object {

    public static final int FIELD_INSTALL=1;
    public static final int FIELD_COMPONENT=2;
    public static final int FIELD_SUBCOMPONENT=3;
    public static final int FIELD_FILE=4;
    public static final int FIELD_KEY=5;
    public static final int FIELD_ORIGINAL_TEXT=6;
    public static final int FIELD_NOTE=7;
    public static final int FIELD_KEEP=8;
    public static final int FIELD_TRANSLATED_TEXT=9;
    public static final int FIELD_QASTATUS=10;
    public static final int FIELD_QACOMMENT=11;
    public static final int FIELD_TRANSLATED_ACCESSKEY=12;
    public static final int FIELD_TRANSLATED_COMMANDKEY=13;
    public static final int FIELD_ORIGINAL_ACCESSKEY=14;
    public static final int FIELD_ORIGINAL_COMMANDKEY=15;
    public static final int FIELD_CURRENT_TEXT=16;
    public static final int FIELD_CURRENT_ACCESSKEY=17;
    public static final int FIELD_CURRENT_COMMANDKEY=18;
    

    private static Class bclass;
    private static Class iclass;
    private static Class sclass;
    private int field;

    public static void init()
    {
        try
        {
            bclass = Class.forName("java.lang.Boolean");
            iclass = Class.forName("java.lang.Integer");
            sclass = Class.forName("java.lang.String");
        }
        catch (Exception e)
        {
            

            Log.write("Error initializing complexcolumn data");
        }
    }

    /** Creates new ComplexColumn */
    public ComplexColumn(int f)
    {
        field=f;
    }

    public int getField()
    {
        return field;
    }

    public String getName()
    {
        String result="";

        switch (field)
        {
            case FIELD_INSTALL:
            result ="Install";
            break;
            case FIELD_COMPONENT:
            result="Component";
            break;
            case FIELD_SUBCOMPONENT:
            result="Subcomponent";
            break;
            case FIELD_FILE:
            result="File";
            break;
            case FIELD_KEY:
            result="Key";
            break;
            case FIELD_ORIGINAL_TEXT:
            result="Org. Text";
            break;
            case FIELD_NOTE:
            result="Localization note";
            break;
            case FIELD_KEEP:
            result="Keep";
            break;
            case FIELD_TRANSLATED_TEXT:
            result="Translated Text";
            break;
            case FIELD_QASTATUS:
            result="QA status";
            break;
            case FIELD_QACOMMENT:
            result="QA comment";
            break;
            case FIELD_ORIGINAL_ACCESSKEY:
            result="Org. accessKey";
            break;
            case FIELD_ORIGINAL_COMMANDKEY:
            result="Org. commandKey";
            break;
            case FIELD_TRANSLATED_ACCESSKEY:
            result ="Translated accesskey";
            break;
            case FIELD_TRANSLATED_COMMANDKEY:
            result = "Translated commandkey";
            break;
            case FIELD_CURRENT_TEXT:
            result = "Current text";
            break;
            case FIELD_CURRENT_ACCESSKEY:
            result = "Current accesskey";
            break;
            case FIELD_CURRENT_COMMANDKEY:
            result = "Current commandkey";
            break;
            
                
            
        }
        return result;
    }

    public Class getColumnClass()
    {
        Class result=null;

        switch (field)
        {
            case FIELD_INSTALL:
            case FIELD_COMPONENT:
            case FIELD_SUBCOMPONENT:
            case FIELD_FILE:
            case FIELD_KEY:
            case FIELD_ORIGINAL_TEXT:
            case FIELD_NOTE:
            case FIELD_TRANSLATED_TEXT:
            case FIELD_QACOMMENT:
            case FIELD_ORIGINAL_ACCESSKEY:
            case FIELD_ORIGINAL_COMMANDKEY:
            case FIELD_TRANSLATED_ACCESSKEY:
            case FIELD_TRANSLATED_COMMANDKEY:
            case FIELD_CURRENT_TEXT:
            case FIELD_CURRENT_ACCESSKEY:
            case FIELD_CURRENT_COMMANDKEY:
            result = sclass;
            break;
            case FIELD_KEEP:
            result = bclass;
            break;
            case FIELD_QASTATUS:
            result = iclass;
        }
        return result;
    }
    public boolean isEditable(Phrase p,String localeName)
    {
        boolean result=false;
        switch (field)
        {
            case FIELD_INSTALL:
            case FIELD_COMPONENT:
            case FIELD_SUBCOMPONENT:
            case FIELD_FILE:
            case FIELD_KEY:
            case FIELD_ORIGINAL_TEXT:
            case FIELD_ORIGINAL_ACCESSKEY:
            case FIELD_ORIGINAL_COMMANDKEY:
            result=false;
            break;
            case FIELD_NOTE:
            case FIELD_KEEP:
            result=true;
            break;
            case FIELD_TRANSLATED_TEXT:
            case FIELD_CURRENT_TEXT:
            if (p.getKeepOriginal())
            {
                result=false;
            }
            else
            {
                result=true;
            }
            break;
            case FIELD_QACOMMENT:
            case FIELD_QASTATUS:
            if (p.getChildByName(localeName)!=null)
            {
                result=true;
            }
            else
            {
                result=false;
            }
            break;
            case FIELD_TRANSLATED_ACCESSKEY:
            case FIELD_CURRENT_ACCESSKEY:
            if (p.getAccessConnection()!=null)
            {
                result=true;
            }
            else
            {
                result=false;
            }
            break;
            case FIELD_TRANSLATED_COMMANDKEY:
            case FIELD_CURRENT_COMMANDKEY:
            if (p.getCommandConnection()!=null)
            {
                result=true;
            }
            else
            {
                result=false;
            }
            break;
        }
        return result;
    }

    public Object getValue(Phrase p,String localeName)
    {
        Object result=null;
        MozInstall install;
        MozComponent component;
        MozFile file;
        Phrase otherPhrase;
        Translation trans;
        switch (field)
        {
            case FIELD_INSTALL:
            result = p.getParent().getParent().getParent().getParent().toString();
            break;
            case FIELD_COMPONENT:
            result = p.getParent().getParent().getParent().toString();
            break;
            case FIELD_SUBCOMPONENT:
            result = p.getParent().getParent().toString();
            break;
            case FIELD_FILE:
            result = p.getParent().toString();
            break;
            case FIELD_KEY:
            result = p.getName();
            break;
            case FIELD_ORIGINAL_TEXT:
            result = p.getText();
            break;
            case FIELD_NOTE:
            result = p.getNote();
            break;
            case FIELD_KEEP:
            result = new Boolean(p.getKeepOriginal());
            break;
            case FIELD_ORIGINAL_ACCESSKEY:
            result="";
            otherPhrase = p.getAccessConnection();
            if (otherPhrase!=null)
            {
               result= otherPhrase.getText();
            }
            break;
            case FIELD_TRANSLATED_ACCESSKEY:
            result="";
            otherPhrase = p.getAccessConnection();
            if (otherPhrase!=null)
            {
                trans= (Translation) otherPhrase.getChildByName(localeName);
                if (trans!=null)
                {
                    result=trans.getText();
                }
            }
            break;
            case FIELD_CURRENT_ACCESSKEY:
            result="";
            otherPhrase = p.getAccessConnection();
            if (otherPhrase!=null)
            {
                trans= (Translation) otherPhrase.getChildByName(localeName);
                if (trans!=null)
                {
                    result=trans.getText();
                }
                else
                {
                    result=otherPhrase.getText();
                }
            }
            break;
                
            case FIELD_ORIGINAL_COMMANDKEY:
            result="";
            otherPhrase = p.getCommandConnection();
            if (otherPhrase!=null)
            {
               result= otherPhrase.getText();
            }
            break;
            case FIELD_TRANSLATED_COMMANDKEY:
            result="";
            otherPhrase = p.getCommandConnection();
            if (otherPhrase!=null)
            {
                trans= (Translation) otherPhrase.getChildByName(localeName);
                if (trans!=null)
                {
                    result=trans.getText(); // get the translated version
                }
            }
            break;
            case FIELD_CURRENT_COMMANDKEY:
            result="";
            otherPhrase = p.getCommandConnection();
            if (otherPhrase!=null)
            {
                trans= (Translation) otherPhrase.getChildByName(localeName);
                if (trans!=null)
                {
                    result=trans.getText();
                }
                else
                {
                    result=otherPhrase.getText();
                }
            }
            break;

            case FIELD_TRANSLATED_TEXT:
            result="";
            trans =  (Translation) p.getChildByName(localeName);
            if (trans!=null)
            {
                
                result = trans.getText();
            }
            break;
            case FIELD_CURRENT_TEXT:
            result="";
            trans =  (Translation) p.getChildByName(localeName);

            if (trans!=null)
            {
                result = trans.getText();
            }
            else
            {
                result = p.getText();
            }
            break;
            case FIELD_QASTATUS:
            trans =  (Translation) p.getChildByName(localeName);

            if (trans!=null)
            {
                result = new Integer(trans.getStatus());
            }
            else
            {
                result = new Integer(Translation.QA_NOTSEEN);
            }
            break;
            case FIELD_QACOMMENT:
            result="";
            trans =  (Translation) p.getChildByName(localeName);

            if (trans!=null)
            {
                result = trans.getText();
            }
            break;
        }
        return result;
    }

    public void setValue(Phrase p,Object value,String localeName)
    {
        String strValue;
        Boolean boolValue;
        Integer intValue;
        Translation pair;
        Phrase otherPhrase;

        switch (field)
        {
            case FIELD_NOTE:
            p.setNote(value.toString());
            break;
            case FIELD_KEEP:
            boolValue = (Boolean) value;
            p.setKeepOriginal(boolValue.booleanValue());
            break;
            case FIELD_TRANSLATED_TEXT:
            case FIELD_CURRENT_TEXT:
            strValue = (String) value;
            if (!strValue.equals(""))
            {
                pair =  (Translation) p.getChildByName(localeName);
                if (pair==null)
                {
                pair = new Translation(localeName,p,strValue);
                    p.addChild(pair);
                }
                else
                {
                    pair.setText(strValue);
                }
            }
            else
            {
                p.removeChild(p.getChildByName(localeName));
            }
            break;
            case FIELD_QASTATUS:
            intValue = (Integer) value;
            pair =  (Translation) p.getChildByName(localeName);
            if (pair!=null)
            {
                pair.setStatus(intValue.intValue());
            }
            break;
            case FIELD_QACOMMENT:
            strValue = (String) value;
            pair =  (Translation) p.getChildByName(localeName);
            if (pair!=null)
            {
                pair.setComment(strValue);
            }
            break;
            case FIELD_TRANSLATED_ACCESSKEY:
            case FIELD_CURRENT_ACCESSKEY:
            strValue = (String) value;
            otherPhrase = p.getAccessConnection();

            if (otherPhrase!=null)
            {
                if (!strValue.equals(""))
                {
                    pair =  (Translation) otherPhrase.getChildByName(localeName);
                    if (pair==null)
                    {
                        pair = new Translation(localeName,otherPhrase,strValue);
                        otherPhrase.addChild(pair);
                    }
                    else
                    {
                        pair.setText(strValue);
                    }
                }
                else
                {
                    
                    otherPhrase.removeChild(otherPhrase.getChildByName(localeName));
                }
            }
            break;
            case FIELD_TRANSLATED_COMMANDKEY:
            case FIELD_CURRENT_COMMANDKEY:
            strValue = (String) value;
            otherPhrase = p.getCommandConnection();

            if (otherPhrase!=null)
            {
                if (!strValue.equals(""))
                {
                    pair =  (Translation) otherPhrase.getChildByName(localeName);
                    if (pair==null)
                    {
                        pair = new Translation(localeName,otherPhrase,strValue);
                        otherPhrase.addChild(pair);
                    }
                    else
                    {
                        pair.setText(strValue);
                    }
                }
                else
                {
                    otherPhrase.removeChild(otherPhrase.getChildByName(localeName));
                }
            }
            break;
        }
    }

    public String toString()
    {
        return getName();
    }
}