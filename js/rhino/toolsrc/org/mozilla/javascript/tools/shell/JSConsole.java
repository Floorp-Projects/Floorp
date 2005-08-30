/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino JavaScript Debugger code, released
 * November 21, 2000.
 *
 * The Initial Developer of the Original Code is See Beyond Corporation.

 * Portions created by See Beyond are
 * Copyright (C) 2000 See Beyond Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Christopher Oliver
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */
package org.mozilla.javascript.tools.shell;
import java.io.*;
import java.awt.event.*;
import javax.swing.*;

public class JSConsole extends JFrame implements ActionListener
{
    static final long serialVersionUID = 2551225560631876300L;

    private File CWD;
    private JFileChooser dlg;
    private ConsoleTextArea consoleTextArea;

    public String chooseFile() {
        if(CWD == null) {
            String dir = System.getProperty("user.dir");
            if(dir != null) {
                CWD = new File(dir);
            }
        }
        if(CWD != null) {
            dlg.setCurrentDirectory(CWD);
        }
        dlg.setDialogTitle("Select a file to load");
        int returnVal = dlg.showOpenDialog(this);
        if(returnVal == JFileChooser.APPROVE_OPTION) {
            String result = dlg.getSelectedFile().getPath();
            CWD = new File(dlg.getSelectedFile().getParent());
            return result;
        }
        return null;
    }

    public static void main(String args[]) {
        JSConsole console = new JSConsole(args);
    }

    public void createFileChooser() {
        dlg = new JFileChooser();
        javax.swing.filechooser.FileFilter filter =
            new javax.swing.filechooser.FileFilter() {
                    public boolean accept(File f) {
                        if(f.isDirectory()) {
                            return true;
                        }
                        String name = f.getName();
                        int i = name.lastIndexOf('.');
                        if(i > 0 && i < name.length() -1) {
                            String ext = name.substring(i + 1).toLowerCase();
                            if(ext.equals("js")) {
                                return true;
                            }
                        }
                        return false;
                    }

                    public String getDescription() {
                        return "JavaScript Files (*.js)";
                    }
                };
        dlg.addChoosableFileFilter(filter);

    }

    public JSConsole(String[] args) {
        super("Rhino JavaScript Console");
        JMenuBar menubar = new JMenuBar();
        createFileChooser();
        String[] fileItems  = {"Load...", "Exit"};
        String[] fileCmds  = {"Load", "Exit"};
        char[] fileShortCuts = {'L', 'X'};
        String[] editItems = {"Cut", "Copy", "Paste"};
        char[] editShortCuts = {'T', 'C', 'P'};
        String[] plafItems = {"Metal", "Windows", "Motif"};
        boolean [] plafState = {true, false, false};
        JMenu fileMenu = new JMenu("File");
        fileMenu.setMnemonic('F');
        JMenu editMenu = new JMenu("Edit");
        editMenu.setMnemonic('E');
        JMenu plafMenu = new JMenu("Platform");
        plafMenu.setMnemonic('P');
        for(int i = 0; i < fileItems.length; ++i) {
            JMenuItem item = new JMenuItem(fileItems[i],
                                           fileShortCuts[i]);
            item.setActionCommand(fileCmds[i]);
            item.addActionListener(this);
            fileMenu.add(item);
        }
        for(int i = 0; i < editItems.length; ++i) {
            JMenuItem item = new JMenuItem(editItems[i],
                                           editShortCuts[i]);
            item.addActionListener(this);
            editMenu.add(item);
        }
        ButtonGroup group = new ButtonGroup();
        for(int i = 0; i < plafItems.length; ++i) {
            JRadioButtonMenuItem item = new JRadioButtonMenuItem(plafItems[i],
                                                                 plafState[i]);
            group.add(item);
            item.addActionListener(this);
            plafMenu.add(item);
        }
        menubar.add(fileMenu);
        menubar.add(editMenu);
        menubar.add(plafMenu);
        setJMenuBar(menubar);
        consoleTextArea = new ConsoleTextArea(args);
        JScrollPane scroller = new JScrollPane(consoleTextArea);
        setContentPane(scroller);
        consoleTextArea.setRows(24);
        consoleTextArea.setColumns(80);
        addWindowListener(new WindowAdapter() {
                public void windowClosing(WindowEvent e) {
                    System.exit(0);
                }
            });
        pack();
        setVisible(true);
        // System.setIn(consoleTextArea.getIn());
        // System.setOut(consoleTextArea.getOut());
        // System.setErr(consoleTextArea.getErr());
        Main.setIn(consoleTextArea.getIn());
        Main.setOut(consoleTextArea.getOut());
        Main.setErr(consoleTextArea.getErr());
        Main.main(args);
    }

    public void actionPerformed(ActionEvent e) {
        String cmd = e.getActionCommand();
        String plaf_name = null;
        if(cmd.equals("Load")) {
            String f = chooseFile();
            if(f != null) {
                f = f.replace('\\', '/');
                consoleTextArea.eval("load(\"" + f + "\");");
            }
        } else if(cmd.equals("Exit")) {
            System.exit(0);
        } else if(cmd.equals("Cut")) {
            consoleTextArea.cut();
        } else if(cmd.equals("Copy")) {
            consoleTextArea.copy();
        } else if(cmd.equals("Paste")) {
            consoleTextArea.paste();
        } else {
            if(cmd.equals("Metal")) {
                plaf_name = "javax.swing.plaf.metal.MetalLookAndFeel";
            } else if(cmd.equals("Windows")) {
                plaf_name = "com.sun.java.swing.plaf.windows.WindowsLookAndFeel";
            } else if(cmd.equals("Motif")) {
                plaf_name = "com.sun.java.swing.plaf.motif.MotifLookAndFeel";
            }
            if(plaf_name != null) {
                try {
                    UIManager.setLookAndFeel(plaf_name);
                    SwingUtilities.updateComponentTreeUI(this);
                    consoleTextArea.postUpdateUI();
                    // updateComponentTreeUI seems to mess up the file
                    // chooser dialog, so just create a new one
                    createFileChooser();
                } catch(Exception exc) {
                    JOptionPane.showMessageDialog(this,
                                                  exc.getMessage(),
                                                  "Platform",
                                                  JOptionPane.ERROR_MESSAGE);
                }
            }
        }

    }

};
