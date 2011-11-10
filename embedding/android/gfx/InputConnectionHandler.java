package org.mozilla.gecko.gfx;

import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.KeyEvent;

public interface InputConnectionHandler
{
    InputConnection onCreateInputConnection(EditorInfo outAttrs);
    boolean onKeyPreIme(int keyCode, KeyEvent event);
    boolean onKeyDown(int keyCode, KeyEvent event);
    boolean onKeyLongPress(int keyCode, KeyEvent event);
    boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event);
    boolean onKeyUp(int keyCode, KeyEvent event);
}
