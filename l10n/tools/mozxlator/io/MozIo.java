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
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.datamodel.*;

/**
 *
 * @author  Henrik
 * @version 4.0
 */
public class MozIo extends Object {
       
    public static MozFileReader getFileReader(MozFile mfil,InputStream is)
    {
        MozFileReader result=null;
        int type;
        
        type = mfil.getType();

        switch (type)
        {
            case MozFile.TYPE_DTD:
                result = new DTDReader(mfil,is);
                break;
            case MozFile.TYPE_PROP:
                result = new PropertiesReader(mfil,is);
                break;
            case MozFile.TYPE_UNSUPPORTED:
                result = null;
                break;
        }
        return result;
    }
    
    public static MozFileWriter getFileWriter(MozFile mfil,OutputStream os)
    {
        MozFileWriter result=null;
        int type;
        
        type = mfil.getType();
        
        switch (type)
        {
            case MozFile.TYPE_DTD:
                result = new DTDWriter(mfil,os);
                break;
            case MozFile.TYPE_PROP:
                result = new PropertiesWriter(mfil,os);
                break;
            case MozFile.TYPE_UNSUPPORTED:
                result = null;
                break;
        }
        return result;
    }

}