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


import java.io.*;
import java.util.*;
import java.util.zip.*;

import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.io.*;
import org.mozilla.translator.kernel.*;
/**
 *
 * @author  Henrik
 * @version
 */
public class ImportLocaleRunner extends Thread {

    MozInstall install;
    String fileName;
    String localeName;

    /** Creates new ImportPartialGlossary */
    public ImportLocaleRunner(MozInstall i,String fn,String ln)
    {
        install=i;
        fileName=fn;
        localeName=ln;
    }

    public void run()
    {
        LocaleAccess pa = new LocaleAccess(install,fileName,localeName);
        pa.load();        
    }
}