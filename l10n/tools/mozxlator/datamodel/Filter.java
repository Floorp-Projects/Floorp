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

import org.mozilla.translator.kernel.*;
/**
 *
 * @author  Henrik
 * @version
 */
public class Filter extends Object {

    public static final int FIELD_KEY=1;
    public static final int FIELD_ORG_TEXT=2;
    public static final int FIELD_NOTE=3;
    public static final int FIELD_COMMENT=4;
    public static final int FIELD_TRANS_TEXT=5;

    public static final int RULE_IS=1;
    public static final int RULE_IS_NOT=2;
    public static final int RULE_CONTAINS=3;
    public static final int RULE_CONTAINS_NOT=4;
    public static final int RULE_STARTS_WITH=5;
    public static final int RULE_ENDS_WITH=6;

    private int rule;
    private int field;
    private int attribute1;
    private boolean caseCheck;
    private String value;

    /** Creates new Filter */
    public Filter(int r,int f,String v,boolean cc)
    {
        rule= r;
        field=f;
        value=v;
        caseCheck=cc;
    }

    public int getRule()
    {
        return rule;
    }

    public int getField()
    {
        return field;
    }

    public String getValue()
    {
        return value;
    }

    public void setRule(int r)
    {
        rule=r;
    }

    public void setField(int f)
    {
        field=f;
    }

    public void setValue(String v)
    {
        value=v;
    }
    
    public boolean check(Phrase phrase)
    {
        return check(phrase,"MT_no_locale");
    }
    
    public boolean check(Phrase phrase,String localeName)
    {
        boolean result;
        String compareText;
        Translation translation = (Translation) phrase.getChildByName(localeName);

        compareText=null;
        result = false;
        switch (field)
        {
            case FIELD_KEY:
            compareText = phrase.getName();
            break;
            case FIELD_ORG_TEXT:
            compareText = phrase.getText();
            break;
            case FIELD_NOTE:
            compareText = phrase.getNote();
            break;
            case FIELD_TRANS_TEXT:
            if (translation!=null)
            {
                compareText=translation.getText();
            }
            break;
            case FIELD_COMMENT:
            if (translation!=null)
            {
                compareText=translation.getComment();
            }
            break;
 
        }
        if (compareText!=null)
        {
            if (!caseCheck)
            {
                compareText=compareText.toLowerCase();
                value=value.toLowerCase();
            }

            switch (rule)
            {
                case RULE_IS:
                if (compareText.equals(value))
                {
                    result=true;
                }
                break;
                case RULE_IS_NOT:
                if (!compareText.equals(value))
                {
                    result=true;
                }
                break;
                case RULE_CONTAINS:
                if (compareText.indexOf(value)!=-1)
                {
                    result=true;
                }
                break;
                case RULE_CONTAINS_NOT:
                if (compareText.indexOf(value)!=-1)
                {
                    result=true;
                }
                break;

                case RULE_STARTS_WITH:
                if (compareText.startsWith(value))
                {
                    result=true;
                }
                break;
                case RULE_ENDS_WITH:
                if (compareText.endsWith(value))
                {
                    result=true;
                }
                break;
            }
        }
        else
        {
            result=false;
        }
        return result;
    }

}
