/*
 * Created on 01 November 2005, 00:34
 *
 * Copyright  2002-2005 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
package grendel.util.ant;

import java.io.File;
import org.apache.tools.ant.Project;
import org.apache.tools.ant.FileScanner;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.DirectoryScanner;
import org.apache.tools.ant.types.selectors.OrSelector;
import org.apache.tools.ant.types.selectors.AndSelector;
import org.apache.tools.ant.types.selectors.NotSelector;
import org.apache.tools.ant.types.selectors.DateSelector;
import org.apache.tools.ant.types.selectors.FileSelector;
import org.apache.tools.ant.types.selectors.NoneSelector;
import org.apache.tools.ant.types.selectors.SizeSelector;
import org.apache.tools.ant.types.selectors.TypeSelector;
import org.apache.tools.ant.types.selectors.DepthSelector;
import org.apache.tools.ant.types.selectors.DependSelector;
import org.apache.tools.ant.types.selectors.ExtendSelector;
import org.apache.tools.ant.types.selectors.SelectSelector;
import org.apache.tools.ant.types.selectors.PresentSelector;
import org.apache.tools.ant.types.selectors.SelectorScanner;
import org.apache.tools.ant.types.selectors.ContainsSelector;
import org.apache.tools.ant.types.selectors.FilenameSelector;
import org.apache.tools.ant.types.selectors.MajoritySelector;
import org.apache.tools.ant.types.selectors.DifferentSelector;
import org.apache.tools.ant.types.selectors.SelectorContainer;
import org.apache.tools.ant.types.selectors.ContainsRegexpSelector;
import org.apache.tools.ant.types.selectors.modifiedselector.ModifiedSelector;

/**
 *
 * @author hash9
 */
public class antRel extends org.apache.tools.ant.types.FileSet {
    
    private String base;
    private String base_prefix="";

    public void setPrefix(String base_prefix) {
        this.base_prefix = base_prefix;
        if (!(base_prefix.charAt(base_prefix.length()-1)==File.separatorChar)) {
            base_prefix += File.separator;
        }
    }
   
    public void setBase(String base) {
        this.base = base;
        if (!(base.charAt(base.length()-1)==File.separatorChar)) {
            base += File.separator;
        }
    }
    
    /**
     * Returns included files as a list of semicolon-separated filenames.
     *
     * @return a <code>String</code> of included filenames.
     */
    public String toString() {
        DirectoryScanner ds = getDirectoryScanner(getProject());
        String[] files = ds.getIncludedFiles();
        StringBuffer sb = new StringBuffer();
        
        for (int i = 0; i < files.length; i++) {
            if (i > 0) {
                sb.append(' ');
            }
            String file = files[i];
            String rfile = null;
            String prefix = "";
            while (file.length()>0) {
                if (file.startsWith(base)) {
                    rfile = prefix + file.substring(base.length()-1);
                    break;
                } else {
                    prefix+=".."+File.separatorChar;
                    int pos = file.lastIndexOf(File.separatorChar);
                    if (pos==-1) {
                        file="";
                    } else {
                        file = file.substring(0,pos);
                    }
                }
            }
            if (rfile==null) {
                rfile=files[i];
            }
            sb.append(base_prefix+rfile);
        }
        return sb.toString();
    }
}
