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
package org.mozilla.translator.runners;

import java.util.*;
import java.io.*;

import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.io.*;
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.gui.*;
/**
 *
 * @author  Henrik
 * @version
 */
public class WritePackageRunner extends Thread {

    private MozInstall install;
    private String fileName;
    private String localeName;
    private String author,display,preview;

    /** Creates new WritePackageRunner */
    public WritePackageRunner(MozInstall i,String fn,String ln,String a,String d,String p)
    {
        install=i;
        fileName=fn;
        localeName=ln;
        author =a;
        display=d;
        preview=p;
    }

    public void run()
    {
        File baseDir;
        File componentDir;
        File subcomponentDir;
        File localeDir;
        File fileFile;
        Iterator componentIterator;
        Iterator subcomponentIterator;
        Iterator fileIterator;
        MozComponent currentComponent;
        MozComponent currentSubComponent;
        MozFile currentFile;
        int filesDone=0;

        MainWindow vindue = MainWindow.getDefaultInstance();
        
        baseDir = new File(fileName);

        makeDir(baseDir);

        componentIterator = install.getChildIterator();

        while (componentIterator.hasNext())
        {
            currentComponent = (MozComponent) componentIterator.next();

            componentDir = new File(baseDir,currentComponent.getName());

            makeDir(componentDir);

            subcomponentIterator = currentComponent.getChildIterator();

            while (subcomponentIterator.hasNext())
            {
                currentSubComponent = (MozComponent) subcomponentIterator.next();
                localeDir = new File(componentDir,"locale");
                makeDir(localeDir);
                if (currentSubComponent.getName().equals("MT_default"))
                {
                    subcomponentDir = localeDir;
                    
                }
                else
                {
                    subcomponentDir = new File(localeDir,currentSubComponent.getName());
                    makeDir(subcomponentDir);

                }

                fileIterator = currentSubComponent.getChildIterator();

                while (fileIterator.hasNext())
                {
                    currentFile =(MozFile) fileIterator.next();
                    vindue.setStatus("Files done: " + filesDone + ", currently writeing: " + currentFile);
                    
                    writeFile(subcomponentDir,currentFile);
                    filesDone++;
                }
            }
        }
        vindue.setStatus("Writeing manifest file");
        writeManifest(baseDir);
        vindue.setStatus("Ready");
        

    }

    private void makeDir(File fil)
    {
        if (!fil.exists())
        {
            fil.mkdir();
        }
    }

    private void writeFile(File dir,MozFile fil)
    {
        MozFileWriter writer;
        FileOutputStream fos;
        Iterator phraseIterator;
        Phrase currentPhrase;
        Translation currentTranslation;
        String key;
        String text;     
        try
        {
            fos = new FileOutputStream(dir + File.separator + fil);
            writer = MozIo.getFileWriter(fil,fos);
            if (writer!=null)
            {
                writer.writeFile(localeName);
            }
            else
            {
                // should really copy the file
            }
        }
        catch (Exception e)
        {
            Log.write("Exception : "+ e);
        }
    }

    private void writeManifest(File dir)
    {
        Iterator componentIterator;
        MozComponent currentComponent;

        try
        {
            File fil = new File(dir,"manifest.rdf");

            FileWriter fw= new FileWriter(fil);

            PrintWriter pw = new PrintWriter(fw);

            pw.println("<?xml version=\"1.0\"?>");
            pw.println("<RDF:RDF xmlns:RDF=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"");
            pw.println("         xmlns:chrome=\"http://www.mozilla.org/rdf/chrome#\">");
            pw.println();

            pw.println("  <RDF:Seq about=\"urn:mozilla:locale:root\">");
            pw.println("    <RDF:li resource=\"urn:mozilla:locale:" + localeName + "\"/>");
            pw.println("  </RDF:Seq>");
            pw.println();

            pw.println("  <RDF:Description about=\"urn:mozilla:locale:" + localeName +"\"");
            pw.println("       chrome:displayName=\"" + display  + "\"");
            pw.println("       chrome:author=\"" + author + "\"");
            pw.println("       chrome:name=\"" + localeName + "\"");
            pw.println("       chrome:previewURL=\"" + preview + "\">");
            pw.println();

            pw.println("    <chrome:packages>");
            pw.println("      <RDF:Seq about=\"urn:mozilla:locale:" + localeName+ ":packages\">");

            componentIterator = install.getChildIterator();

            while (componentIterator.hasNext())
            {
                currentComponent = (MozComponent) componentIterator.next();

                pw.println("        <RDF:li resource=\"urn:mozilla:locale:" + localeName + ":" + currentComponent.getName() + "\"/>");
            }
            pw.println("      </RDF:Seq>");
            pw.println("    </chrome:packages>");
            pw.println("  </RDF:Description>");
            pw.println("</RDF:RDF>");

            pw.close();
        }
        catch (Exception e)
        {
            Log.write("Error writing manifest file");
        }
        
    }

}