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
package org.mozilla.translator.actions;

import java.awt.event.*;
import javax.swing.*;
import java.util.*;
import java.io.*;


import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.gui.*;
import org.mozilla.translator.gui.dialog.*;
import org.mozilla.translator.gui.models.*;
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.runners.*;
import org.mozilla.translator.fetch.*;
/**
 *
 * @author  Henrik
 * @version
 */
public class AdvancedSearchAction extends AbstractAction {

    private int rule;
    private int column;
    private String rul;
    private String col;
    private boolean cc;
    

    /** Creates new SearchViewAction */
    public AdvancedSearchAction()
    {
        super("Advanced search",null);
    }

    public void actionPerformed(ActionEvent evt)
    {
        Filter firstSearch,secondSearch,thirdSearch;
        List collectedList;
        String lname,text;
        MainWindow mw = MainWindow.getDefaultInstance();
        AdvancedSearchDialog sd = new AdvancedSearchDialog(mw,true);
        
        
        boolean okay = sd.visDialog();

        if (okay)
        {


            if (sd.getFirstEnabled())
            {          
                rule= 0;
                column =0;
                rul = sd.getFirstRule();
                col = sd.getFirstColumn();
                lname = sd.getFirstLocaleName();
                text = sd.getFirstSearchText();
                cc = sd.getFirstCase();
                
                assignRule(Filter.RULE_IS,"Is");
                assignRule(Filter.RULE_IS_NOT,"Is not");
                assignRule(Filter.RULE_CONTAINS,"Contains");
                assignRule(Filter.RULE_CONTAINS_NOT,"Doesn't contain");
                assignRule(Filter.RULE_STARTS_WITH,"Starts with");
                assignRule(Filter.RULE_ENDS_WITH,"Ends with");

                assignColumn(Filter.FIELD_KEY,"Key");
                assignColumn(Filter.FIELD_NOTE,"Localization note");
                assignColumn(Filter.FIELD_ORG_TEXT,"Original text");
                assignColumn(Filter.FIELD_TRANS_TEXT,"Translated text");
                assignColumn(Filter.FIELD_COMMENT,"QA comment");
                
                firstSearch = new Filter(rule,column,text,cc,lname);
                
            }
            else
            {
                firstSearch =null;
            }

            if (sd.getSecondEnabled())
            {          
                rule= 0;
                column =0;
                rul = sd.getSecondRule();
                col = sd.getSecondColumn();
                lname = sd.getSecondLocaleName();
                text = sd.getSecondSearchText();
                cc = sd.getSecondCase();
                
                assignRule(Filter.RULE_IS,"Is");
                assignRule(Filter.RULE_IS_NOT,"Is not");
                assignRule(Filter.RULE_CONTAINS,"Contains");
                assignRule(Filter.RULE_CONTAINS_NOT,"Doesn't contain");
                assignRule(Filter.RULE_STARTS_WITH,"Starts with");
                assignRule(Filter.RULE_ENDS_WITH,"Ends with");

                assignColumn(Filter.FIELD_KEY,"Key");
                assignColumn(Filter.FIELD_NOTE,"Localization note");
                assignColumn(Filter.FIELD_ORG_TEXT,"Original text");
                assignColumn(Filter.FIELD_TRANS_TEXT,"Translated text");
                assignColumn(Filter.FIELD_COMMENT,"QA comment");
                
                
                secondSearch = new Filter(rule,column,text,cc,lname);
            }
            else
            {
                secondSearch =null;
            }            

            
            if (sd.getThirdEnabled())
            {          
                rule= 0;
                column =0;
                rul = sd.getThirdRule();
                col = sd.getThirdColumn();
                lname = sd.getThirdLocaleName();
                text = sd.getThirdSearchText();
                cc = sd.getThirdCase();
                
                assignRule(Filter.RULE_IS,"Is");
                assignRule(Filter.RULE_IS_NOT,"Is not");
                assignRule(Filter.RULE_CONTAINS,"Contains");
                assignRule(Filter.RULE_CONTAINS_NOT,"Doesn't contain");
                assignRule(Filter.RULE_STARTS_WITH,"Starts with");
                assignRule(Filter.RULE_ENDS_WITH,"Ends with");

                assignColumn(Filter.FIELD_KEY,"Key");
                assignColumn(Filter.FIELD_NOTE,"Localization note");
                assignColumn(Filter.FIELD_ORG_TEXT,"Original text");
                assignColumn(Filter.FIELD_TRANS_TEXT,"Translated text");
                assignColumn(Filter.FIELD_COMMENT,"QA comment");
                
                thirdSearch = new Filter(rule,column,text,cc,lname);
            }
            else
            {
                thirdSearch =null;
            }            

            ShowWhatDialog swd = new ShowWhatDialog();
            swd.disableLocaleField();
            if (swd.visDialog())                
            {
                String localeName = swd.getSelectedLocale();
                List cols = swd.getSelectedColumns();
                Fetcher sf= new FetchAdvancedSearch(firstSearch,secondSearch,thirdSearch,true);
                collectedList  = FetchRunner.getFromGlossary(sf);
                Collections.sort(collectedList);
                ComplexTableWindow ctw = new ComplexTableWindow("Found Strings",collectedList,cols,localeName);
            }
        }        
    }

    private void assignRule(int value,String comp)
    {
        if (rul.equals(comp))
        {
            rule = value;
        }
    }

    private void assignColumn(int value,String comp)
    {
        if (col.equals(comp))
        {
            column = value;
        }
    }

}