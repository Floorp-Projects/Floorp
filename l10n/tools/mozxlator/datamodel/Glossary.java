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

import java.util.*;
import java.io.*;
import javax.swing.*;
import javax.swing.tree.*;
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.gui.*;
import org.mozilla.translator.runners.*;
/**
 *
 * @author  Henrik Lynggaard
 * @version 5.0
 */
public class Glossary extends MozTreeNode
{
    private static Glossary instance;

    public static void init()
    {
        instance = new Glossary();
    }

    public static Glossary getDefaultInstance()
    {
        return instance;
    }

    private Glossary()
    {
        super("MozillaTranslator Glossary",null);
        File glosFile;

        glosFile = new File(Settings.getString("System.Glossaryfile","glossary.zip"));
        if (glosFile.exists())
        {
            LoadGlossaryRunner lgr= new LoadGlossaryRunner();
            lgr.start();
        }
        else
        {
            MainWindow.getDefaultInstance().setStatus("Ready");
        }
    }

    // this needs rewrite!!
    
    public DefaultMutableTreeNode getTreeRoot()
    {
        DefaultMutableTreeNode root;
        DefaultMutableTreeNode installNode;
        DefaultMutableTreeNode componentNode;
        DefaultMutableTreeNode subcomponentNode;
        DefaultMutableTreeNode fileNode;

        Iterator installIterator;
        Iterator componentIterator;
        Iterator subcomponentIterator;
        Iterator fileIterator;

        MozInstall currentInstall;
        MozComponent currentComponent,currentSubcomponent;
        MozFile currentFile;

        root= new DefaultMutableTreeNode("Root");
        installIterator = children.iterator();

        while (installIterator.hasNext())
        {
            currentInstall = (MozInstall) installIterator.next();
            
            installNode = new DefaultMutableTreeNode(currentInstall);
            root.add(installNode);

            componentIterator = currentInstall.getChildIterator();

            while (componentIterator.hasNext())
            {
                currentComponent = (MozComponent) componentIterator.next();

                componentNode = new DefaultMutableTreeNode(currentComponent);
                installNode.add(componentNode);

                subcomponentIterator = currentComponent.getChildIterator();

                while (subcomponentIterator.hasNext())
                {
                    currentSubcomponent = (MozComponent) subcomponentIterator.next();

                    subcomponentNode = new DefaultMutableTreeNode(currentSubcomponent);
                    componentNode.add(subcomponentNode);

                    fileIterator = currentSubcomponent.getChildIterator();

                    while (fileIterator.hasNext())
                    {
                        currentFile = (MozFile) fileIterator.next();

                        fileNode = new DefaultMutableTreeNode(currentFile);
                        subcomponentNode.add(fileNode);
                    }
                }
            }
        }
        return root;
    }

}