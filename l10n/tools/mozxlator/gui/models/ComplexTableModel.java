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

import java.util.*;
import javax.swing.table.*;

import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.kernel.*;
/**
 *
 * @author  Henrik Lynggaard Hansen
 * @version 4.0
 */
public class ComplexTableModel extends AbstractTableModel {

    private List phrases;
    private List columns;
    private int rows;
    private int cols;
    private ComplexColumn cc;
    private String localeName;


    /** Creates new ComplexTableModel */
    public ComplexTableModel(List toModel,List toCols,String lName)
    {
        phrases = toModel;
        columns = toCols;
        localeName=lName;

        rows = phrases.size();
        cols = columns.size();
    }

    public int getRowCount()
    {
        return rows;
    }

    public int getColumnCount()
    {
        return cols;
    }

    public String getColumnName(int index)
    {
        String result;
        cc = (ComplexColumn) columns.get(index);

        result = cc.getName();

        return result;
    }
    public Class getColumnClass(int index)
    {
        Class result;
        cc = (ComplexColumn) columns.get(index);

        result = cc.getColumnClass();

        return result;
    }

    public boolean isCellEditable(int rowIndex,int columnIndex)
    {
        cc = (ComplexColumn) columns.get(columnIndex);

        Phrase ph = (Phrase) phrases.get(rowIndex);

        return cc.isEditable(ph,localeName);
    }

    public Object getValueAt(int rowIndex,int columnIndex)
    {
        cc = (ComplexColumn) columns.get(columnIndex);

        Phrase ph = (Phrase) phrases.get(rowIndex);

        return cc.getValue(ph,localeName);
    }

    public void setValueAt(Object aValue,int rowIndex,int columnIndex)
    {
        cc = (ComplexColumn) columns.get(columnIndex);

        Phrase ph = (Phrase) phrases.get(rowIndex);

        cc.setValue(ph,aValue,localeName);

    }
    public String getLocaleName()
    {
        return localeName;
    }

    public Phrase getRow(int index)
    {
        return (Phrase)  phrases.get(index);
    }
}