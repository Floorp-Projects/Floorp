/** This is the DiaogClient interface 
 *  for making client callbacks to the
 *  parent of a non-modal dialog box
 */

package org.mozilla.webclient.test;

import java.awt.*;

public interface DialogClient {
  void dialogDismissed(Dialog d);
  void dialogCancelled(Dialog d);
}
