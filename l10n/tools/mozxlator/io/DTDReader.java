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
package org.mozilla.translator.io;

import java.io.*;
import java.util.*;
import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.kernel.*;
/**
 *
 * @author  Henrik
 * @version
 */
public class DTDReader extends  MozFileReader {


    private static final int STATUS_COMMENT_WAITING=1;
    private static final int STATUS_COMMENT_BEGUN=2;
    private static final int STATUS_COMMENT_JUMP=3;
    private static final int STATUS_COMMENT_SCOPE=4;
    private static final int STATUS_COMMENT_NOTE=5;

    private static final int STATUS_ENTITY_KEY=6;
    private static final int STATUS_ENTITY_TEXT=7;

    private static final int SCOPE_FILE=1;
    private static final int SCOPE_BLOCK=2;
    private static final int SCOPE_KEY=3;

    private static final String TOKEN_ENTITY="!ENTITY";
    private static final String TOKEN_COMMENT="!--";
    private static final String TOKEN_LOCNOTE="LOCALIZATION";
    private static final String TOKEN_BEGINBLOCK="BEGIN";
    private static final String TOKEN_ENDBLOCK="END";

    
    private InputStreamReader isr;
    private BufferedReader br;
    private boolean globalDone;
    private String key,text;

    public DTDReader(MozFile f,InputStream i)
    {
        super(f,i);
    }

    
    public void readFile(String localeName,List changeList) throws IOException
    {
        boolean done;
        Phrase currentPhrase;
        Translation currentTranslation;
        
        
        isr = new InputStreamReader(is,"UTF-8");
        br = new BufferedReader(isr);
        globalDone=false;
        
        while (!globalDone)
       {
           readNextEntry(localeName);
           if (!key.equals(""))
            {
                currentPhrase = (Phrase) fil.getChildByName(key);
            
                if (localeName.equalsIgnoreCase("en-us"))
                {
                    if (currentPhrase==null)
                    {
                        currentPhrase= new Phrase(key,fil,text,"",false);
                        fil.addChild(currentPhrase);
                        changeList.add(currentPhrase);
                    }
                    else
                    {
                        if (!currentPhrase.getText().equals(text))
                        {
                            currentPhrase.setText(text);
                            changeList.add(currentPhrase);
                        }
                            
                    }
                    currentPhrase.setMarked(true);
                    
               }
                else
                {
                    if (currentPhrase!=null)
                    {
                        currentTranslation = (Translation) currentPhrase.getChildByName(localeName);
                        if (currentTranslation==null)
                        {
                            currentTranslation = new Translation(localeName,currentPhrase,text);
                            currentPhrase.addChild(currentTranslation);
                        }
                        else
                        {
                            currentTranslation.setText(text);
                        }
                    }
                }
            }
        }
    }        

    
    public void readNextEntry(String localeName)
    {
        int number;
        StringBuffer segmentBuffer;
        boolean entityBegun;
        char letter;
        boolean entryDone;
        boolean endResult;
        
        entryDone=false;
        segmentBuffer =null;
        entityBegun=false;
        
        while (!entryDone)
        {
            number = readNextChar();
            if (number!=-1)
            {
                letter = (char) number;
                
                if (entityBegun==true)
                {
                    segmentBuffer.append(letter);
                    
                    if (segmentBuffer.toString().equalsIgnoreCase(TOKEN_COMMENT))
                    {
                        readComment();
                        entityBegun=false;
                    }
                    else if (segmentBuffer.toString().equalsIgnoreCase(TOKEN_ENTITY))
                    {
                        readEntity();
                        entryDone=true;
                    }
                    else if (letter=='>')
                    {
                        entityBegun=false;
                    }
                }
                else
                {
                    if (letter=='<')
                    {
                        entityBegun=true;
                        segmentBuffer = new StringBuffer();
                    }  
                }
            }
            else
            {
                entryDone=true;
                globalDone=true;
            }
        }
    }

    public void readEntity()
    {
        boolean keyBegun;
        boolean textBegun;
        boolean more;
        int status,number;
        char letter,endChar;
        StringBuffer entityBuffer=null;
        
        status = STATUS_ENTITY_KEY;

        keyBegun = false;
        textBegun = false;
        more=true;
        endChar='z';
        
        while (more)
        {
            number = readNextChar();

            if (number!=-1)
            {
                letter = (char) number;

                switch (status)
                {
                    case STATUS_ENTITY_KEY:
                        if (keyBegun)
                        {
                            if (letter==' ' || letter=='\t')
                            {
                                key = entityBuffer.toString();
                                status = STATUS_ENTITY_TEXT;
                                textBegun=false;
                            }
                            else
                            {
                                entityBuffer.append(letter);
                            }
                        }
                        else
                        {
                            if (letter!=' ' && letter!='\t')
                            {
                                keyBegun= true;
                                entityBuffer= new StringBuffer();
                                entityBuffer.append(letter);
                            }
                        }
                        break;
                    case STATUS_ENTITY_TEXT:
                        if (textBegun)
                        {
                            if (letter==endChar)
                            {
                                text = entityBuffer.toString();
                                readSkip();
                                more=false;
                                
                            }
                            else
                            {
                                entityBuffer.append(letter);
                            }
                        }
                        else
                        {
                            if (letter=='\"' || letter=='\'')
                            {
                                textBegun=true;
                                entityBuffer= new StringBuffer();
                                endChar=letter;
                            }
                        }
                        break;
                }
            }
            else
            {
                globalDone=true;
                more=false;
            }
        }

    }




    private  void readComment()
    {
        readSkip();
    }

    private void readSkip()
    {
        boolean more;
        int number;
        char letter;

        more=true;
        while (more)
        {

            number = readNextChar();
            if (number!=-1)
            {
                letter = (char) number;

                if (letter=='>')
                {
                    more=false;
                }
            }
            else
            {
                more=false;
                globalDone=true;
            }
        }
    }

    private int readNextChar()
    {
        int number;

        try
        {
            number = br.read();
        }
        catch (Exception e)
        {
            Log.write("Error during reading dtd file");
            Log.write("Exception: " + e);
            number=-1;
        }
        return number;
    }
    
    public void close()
    {
        try
        {
            br.close();
        }
        catch (Exception e)
        {
            Log.write("Error  closing read DTD file");
            Log.write("Exception "+e);
        }
    }
}