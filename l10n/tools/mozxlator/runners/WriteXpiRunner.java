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
import java.util.zip.*;
import java.io.*;

import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.io.*;
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.gui.*;
/**
 *
 * @author  Henrik
 * @version 4.1
 */
public class WriteXpiRunner extends Thread {

    private MozInstall install;
    private String fileName;
    private String localeName;
    private String author,display,preview;
    private String version;

    private File zipFile;
    private FileOutputStream fos;
    private ZipOutputStream zos;
    private BufferedOutputStream bos;


    private String entryName;

    /** Creates new WritePackageRunner */
    public WriteXpiRunner(MozInstall i,String fn,String ln,String a,String d,String p,String v)
    {
        install=i;
        fileName=fn;
        localeName=ln;
        author =a;
        display=d;
        preview=p;
        version=v;
    }

    public void run()
    {
        MainWindow vindue = MainWindow.getDefaultInstance();
        
        String componentPrefix;
        String subcomponentPrefix;
        String filePrefix;

        Iterator componentIterator;
        Iterator subcomponentIterator;
        Iterator fileIterator;
        MozComponent currentComponent;
        MozComponent currentSubComponent;
        MozFile currentFile;
        int filesDone=0;
        
        try
        {

            zipFile = new File(fileName);
            fos = new  FileOutputStream(zipFile);
            zos = new ZipOutputStream(fos);
            bos = new BufferedOutputStream(zos);


            componentIterator = install.getChildIterator();

            while (componentIterator.hasNext())
            {
                currentComponent = (MozComponent) componentIterator.next();

                componentPrefix = localeName + File.separator + currentComponent.getName();

                subcomponentIterator = currentComponent.getChildIterator();

                while (subcomponentIterator.hasNext())
                {
                    currentSubComponent = (MozComponent) subcomponentIterator.next();

                    subcomponentPrefix = componentPrefix + File.separator + "locale";

                    if (!currentSubComponent.getName().equals("MT_default"))
                    {
                        subcomponentPrefix = subcomponentPrefix + File.separator + currentSubComponent.getName();
                    }

                    fileIterator = currentSubComponent.getChildIterator();

                    while (fileIterator.hasNext())
                    {
                        currentFile =(MozFile) fileIterator.next();

                        entryName = "bin" + File.separator + "chrome" + File.separator + "locales" + File.separator;
                        entryName = entryName + subcomponentPrefix + File.separator + currentFile.getName();
                        vindue.setStatus("files done: " + filesDone + ", currently packing: " + currentFile);
                        writeFile(currentFile);
                        filesDone++;
                    }
                }
            }
            writeManifest();
            writeInstallScript();
            bos.close();
        }
        catch (Exception e)
        {
            Log.write("Error writing xpi file");
            Log.write("Exception: " +e);
        }
        MainWindow.getDefaultInstance().setStatus("Ready");
    }

    private void writeFile(MozFile fil) throws IOException
    {
        File temp;
        MozFileWriter writer;
        FileOutputStream fos;
        MozInstall install;
        MozComponent comp,subcomp;
        String origFilename;
        
        temp = File.createTempFile("MT_",null);
        fos = new FileOutputStream(temp);
        
        writer = MozIo.getFileWriter(fil,fos);
        if (writer!=null) 
        {         
            writer.writeFile(localeName);
            copyFile(temp,entryName);
            temp.delete();
        }
        else
        {
            subcomp = (MozComponent) fil.getParent();
            comp = (MozComponent) subcomp.getParent();
            install = (MozInstall) comp.getParent();
            origFilename = install.getPath() + File.separator + comp.getName();
            origFilename = origFilename + File.separator + "locale";
            
            if (!subcomp.getName().equals("MT_default"))
            {
                origFilename = origFilename + File.separator + subcomp.getName();
            }
            origFilename = origFilename + File.separator + fil.getName();
            File orig = new File(origFilename);
            
            copyFile(orig,entryName);
        }
    }

    private void writeManifest()
    {
        Iterator componentIterator;
        MozComponent currentComponent;

        try
        {
            File fil = File.createTempFile("MT_",null);

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

            copyFile(fil,"bin" + File.separator + "chrome" + File.separator + "locales" + File.separator + localeName + File.separator + "manifest.rdf");
            fil.delete();
        }
        catch (Exception e)
        {
            Log.write("Error writing manifest file");
        }

    }

    private void writeInstallScript()
    {
        Iterator componentIterator;
        MozComponent currentComponent;

        try
        {
            File fil = File.createTempFile("MT_",null);

            FileWriter fw= new FileWriter(fil);

            PrintWriter pw = new PrintWriter(fw);

            // the text in the file
            pw.println("// this function verifies disk space in kilobytes");
            pw.println("function verifyDiskSpace(dirPath, spaceRequired)");
            pw.println("{");
            pw.println("\t var spaceAvailable;");
            pw.println();
            pw.println("\t // Get the available disk space on the given path");
            pw.println("\t spaceAvailable = fileGetDiskSpaceAvailable(dirPath);");
            
            pw.println("\t // Convert the available disk space into kilobytes");
            pw.println("\t spaceAvailable = parseInt(spaceAvailable / 1024);");

            pw.println("\t // do the verification");
            pw.println("\t if(spaceAvailable < spaceRequired)");
            pw.println("\t {");
            pw.println("\t\t logComment(\"Insufficient disk space: \" + dirPath);");
            pw.println("\t\t logComment(\"  required : \" + spaceRequired + \" K\");");
            pw.println("\t\t logComment(\"  available: \" + spaceAvailable + \" K\");");
            pw.println("\t\t return(false);");
            pw.println("\t }");
            pw.println("\t return(true);");
            pw.println("}");
            pw.println("var srDest = 643;");
            pw.println("var err;");
            pw.println("var fProgram;");

            pw.println("// --- LOCALIZATION NOTE: translate only these ---");
            pw.println("var prettyName = \"" +display + "\";");
            pw.println("var regName    = \"locales/mozilla/"+ localeName +"\";");
            pw.println("var chromeName = \"locales/" + localeName +"\";");
            pw.println("// --- END LOCALIZABLE RESOURCES ---");

            pw.println("err = initInstall(prettyName, regName, \"5.0.0.0000000000\");");
            pw.println("logComment(\"initInstall: \" + err);");

            pw.println("fProgram = getFolder(\"Program\");");
            pw.println("logComment(\"fProgram: \" + fProgram);");

            pw.println("if (verifyDiskSpace(fProgram, srDest))");
            pw.println("{");
            pw.println("\t err = addDirectory(\"\",");
            pw.println("\t\t \"5.0.0.0000000000\",");
            pw.println("\t\t \"bin\",");
            pw.println("\t\t fProgram,");
            pw.println("\t\t \"\",");
            pw.println("\t\t true);");
            pw.println("\t logComment(\"addDirectory() returned: \" + err);");

            pw.println("\t // register chrome");
            pw.println("\t var cf = getFolder(\"Chrome\");");
            pw.println("\t registerChrome(LOCALE | DELAYED_CHROME, getFolder(cf, chromeName));");

            pw.println("\t if (err == SUCCESS)");
            pw.println("\t {");
            pw.println("\t err = performInstall(); ");
            pw.println("\t logComment(\"performInstall() returned: \" + err);");
            pw.println("\t }");
            pw.println("\t else");
            pw.println("\t {");
            pw.println("\t cancelInstall(err);");
            pw.println("\t logComment(\"cancelInstall due to error: \" + err);");
            pw.println("\t }");
            pw.println("}");
            pw.println("else");
            pw.println("\t cancelInstall(INSUFFICIENT_DISK_SPACE);");            
            // end of text
            pw.close();
            copyFile(fil,"install.js");
            fil.delete();            
        }
        catch (Exception e)
        {
            Log.write("Error writing installScript file");
        }

    }

    private void copyFile(File tempFile,String eName) throws IOException
    {
        ZipEntry ze;
        FileInputStream fis;
        BufferedInputStream bis;
        boolean theEnd;
        int trans;
        ze = new ZipEntry(eName);
        zos.putNextEntry(ze);

        fis = new FileInputStream(tempFile);
        bis = new BufferedInputStream(fis);
        theEnd=false;

        while (!theEnd)
        {
            trans = bis.read();
            if (trans==-1)
            {
                theEnd = true;
            }
            else
            {
                bos.write(trans);
            }
        }
        bis.close();
        bos.flush();
    }



}
