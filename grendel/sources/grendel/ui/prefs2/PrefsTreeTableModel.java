/*
 * AddressCardTreeTable.java
 *
 * Created on 08 October 2005, 17:26
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.ui.prefs2;

import grendel.prefs.Preferences;
import grendel.prefs.xml.XMLPreferences;

import grendel.ui.PrefsDialog;
import grendel.widgets.TreeTableModelListener;
import javax.swing.event.TreeExpansionEvent;
import javax.swing.event.TreeModelEvent;
import javax.swing.tree.TreeNode;

import org.jdesktop.swing.treetable.DefaultTreeTableModel;
import org.jdesktop.swing.treetable.TreeTableModel;

import javax.swing.JOptionPane;


/**
 *
 * @author hash9
 */
public class PrefsTreeTableModel extends DefaultTreeTableModel
        implements TreeTableModel {
    /**
     * Creates a new instance of PrefsTreeTableModel
     */
    public PrefsTreeTableModel() {
        this(Preferences.getPreferances());
    }
    
    public PrefsTreeTableModel(XMLPreferences prefs) {
        super(new PrefsTreeNode(null, "Preferences", prefs));
    }
    
    public boolean isCellEditable(Object object, int i) {
        if (i == 2) {
            if (object instanceof PrefsObjectTreeNode) {
                PrefsObjectTreeNode on = (PrefsObjectTreeNode) object;
                
                if (on.getValue() instanceof String) {
                    return true;
                }
                
                if (on.getValue() instanceof Integer) {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    public int getColumnCount() {
        return 3;
    }
    
    public String getColumnName(int i) {
        switch (i) {
            case 0:
                return "key";
                
            case 1:
                return "type";
                
            case 2:
                return "value";
                
            default:
                return "";
        }
    }
    
    public void setValueAt(Object object, Object object0, int i) {
        PrefsObjectTreeNode cn = (PrefsObjectTreeNode) object0;
        PrefsTreeNode n = (PrefsTreeNode) cn.getParent();
        
        if (cn.getValue() instanceof String) {
            n.getPrefs().setProperty((String) cn.getUserObject(), (String) object);
            nodeChanged(n,cn);
        } else if (cn.getValue() instanceof Integer) {
            n.getPrefs().setProperty((String) cn.getUserObject(),
                    Integer.parseInt((String) object));
            nodeChanged(n,cn);
        }
    }
    
    private void nodeChanged(PrefsTreeNode n,PrefsObjectTreeNode cn) {
        for (int i = 0; i < n.getChildCount(); i++) {
            TreeNode ncn = n.getChildAt(i);
            if (cn.equals(ncn)) {
                nodesChanged(n,new int[] {i});
                return;
            }
        }
        return;
    }
    
    public Object getValueAt(Object object, int i) {
        if (object instanceof PrefsObjectTreeNode) {
            PrefsObjectTreeNode n = (PrefsObjectTreeNode) object;
            Object value = n.getValue();
            
            if (i == 1) {
                return value.getClass().getCanonicalName();
            } else if (i == 2) {
                return value.toString();
            }
        } else if (object instanceof PrefsTreeNode) {
            PrefsTreeNode n = (PrefsTreeNode) object;
            Object value = n.getPrefs();
            
            if (i == 1) {
                return value.getClass().getCanonicalName();
            } else if (i == 2) {
                return "{count: "+n.getChildCount()+"}";
            }
        }
        return "";
    }
}
