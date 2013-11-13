/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.lang.reflect.InvocationHandler;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.view.View;
import android.util.Log;

import org.json.*;

import com.jayway.android.robotium.solo.Solo;

public class FennecNativeDriver implements Driver {
    private static final int FRAME_TIME_THRESHOLD = 25;     // allow 25ms per frame (40fps)

    // Map of IDs to element names.
    private HashMap mLocators = null;
    private Activity mActivity;
    private Solo mSolo;
    private String mRootPath;

    private static String mLogFile = null;
    private static LogLevel mLogLevel = LogLevel.INFO;

    // Objects for reflexive access of fennec classes.
    private ClassLoader mClassLoader;
    private Class mApiClass;
    private Class mEventListenerClass;
    private Class mPanningPerfClass;
    private Method mRegisterEventListener;
    private Method mGetPixels;
    private Method mStartFrameRecording;
    private Method mStopFrameRecording;
    private Method mStartCheckerboardRecording;
    private Method mStopCheckerboardRecording;
    private Object mRobocopApi;

    public enum LogLevel {
        DEBUG(1),
        INFO(2),
        WARN(3),
        ERROR(4);

        private int mValue;
        LogLevel(int value) {
            mValue = value;
        }
        public boolean isEnabled(LogLevel configuredLevel) {
            return mValue >= configuredLevel.getValue();
        }
        private int getValue() {
            return mValue;
        }
    }

    public FennecNativeDriver(Activity activity, Solo robocop, String rootPath) {
        mActivity = activity;
        mSolo = robocop;
        mRootPath = rootPath;

        // Set up table of fennec_ids.
        mLocators = convertTextToTable(getFile(mRootPath + "/fennec_ids.txt"));

        // Set up reflexive access of java classes and methods.
        try {
            mClassLoader = activity.getClassLoader();

            mApiClass = mClassLoader.loadClass("org.mozilla.gecko.RobocopAPI");
            mEventListenerClass = mClassLoader.loadClass("org.mozilla.gecko.util.GeckoEventListener");
            mPanningPerfClass = mClassLoader.loadClass("org.mozilla.gecko.gfx.PanningPerfAPI");

            mRegisterEventListener = mApiClass.getMethod("registerEventListener", String.class, mEventListenerClass);
            mGetPixels = mApiClass.getMethod("getViewPixels", View.class);
            mStartFrameRecording = mPanningPerfClass.getDeclaredMethod("startFrameTimeRecording");
            mStopFrameRecording = mPanningPerfClass.getDeclaredMethod("stopFrameTimeRecording");
            mStartCheckerboardRecording = mPanningPerfClass.getDeclaredMethod("startCheckerboardRecording");
            mStopCheckerboardRecording = mPanningPerfClass.getDeclaredMethod("stopCheckerboardRecording");

            mRobocopApi = mApiClass.getConstructor(Activity.class).newInstance(activity);
        } catch (Exception e) {
            log(LogLevel.ERROR, e);
        }
    }

    //Information on the location of the Gecko Frame.
    private boolean mGeckoInfo = false;
    private int mGeckoTop = 100;
    private int mGeckoLeft = 0;
    private int mGeckoHeight= 700;
    private int mGeckoWidth = 1024;

    private void getGeckoInfo() {
        View geckoLayout = mActivity.findViewById(Integer.decode((String)mLocators.get("gecko_layout")));
        if (geckoLayout != null) {
            int[] pos = new int[2];
            geckoLayout.getLocationOnScreen(pos);
            mGeckoTop = pos[1];
            mGeckoLeft = pos[0];
            mGeckoWidth = geckoLayout.getWidth();
            mGeckoHeight = geckoLayout.getHeight();
            mGeckoInfo = true;
        } else {
            throw new RoboCopException("Unable to find view gecko_layout");
        }
    }

    public int getGeckoTop() {
        if (!mGeckoInfo) {
            getGeckoInfo();
        }
        return mGeckoTop;
    }

    public int getGeckoLeft() {
        if (!mGeckoInfo) {
            getGeckoInfo();
        }
        return mGeckoLeft;
    }

    public int getGeckoHeight() {
        if (!mGeckoInfo) {
            getGeckoInfo();
        }
        return mGeckoHeight;
    }

    public int getGeckoWidth() {
        if (!mGeckoInfo) {
            getGeckoInfo();
        }
        return mGeckoWidth;
    }

    /** Find the named element in the list of known Fennec views. 
     *  @return An Element representing the view, or null if the view is not found.
     */
    public Element findElement(Activity activity, String name) {
        if (name == null) {
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR,
                "Can not findElements when passed a null");
            return null;
        }
        if (mLocators.containsKey(name)) {
            return new FennecNativeElement(Integer.decode((String)mLocators.get(name)), activity, mSolo);
        }
        FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR,
            "findElement: Element '"+name+"' does not exist in the list");
        return null;
    }

    public void startFrameRecording() {
        try {
            mStartFrameRecording.invoke(null);
        } catch (IllegalAccessException e) {
            log(LogLevel.ERROR, e);
        } catch (InvocationTargetException e) {
            log(LogLevel.ERROR, e);
        }
    }

    public int stopFrameRecording() {
        try {
            List<Long> frames = (List<Long>)mStopFrameRecording.invoke(null);
            int badness = 0;
            for (int i = 1; i < frames.size(); i++) {
                long frameTime = frames.get(i) - frames.get(i - 1);
                int delay = (int)(frameTime - FRAME_TIME_THRESHOLD);
                // for each frame we miss, add the square of the delay. This
                // makes large delays much worse than small delays.
                if (delay > 0) {
                    badness += delay * delay;
                }
            }
            // Don't do any averaging of the numbers because really we want to
            // know how bad the jank was at its worst
            return badness;
        } catch (IllegalAccessException e) {
            log(LogLevel.ERROR, e);
        } catch (InvocationTargetException e) {
            log(LogLevel.ERROR, e);
        }

        // higher values are worse, and the test failing is the worst!
        return Integer.MAX_VALUE;
    }

    public void startCheckerboardRecording() {
        try {
            mStartCheckerboardRecording.invoke(null);
        } catch (IllegalAccessException e) {
            log(LogLevel.ERROR, e);
        } catch (InvocationTargetException e) {
            log(LogLevel.ERROR, e);
        }
    }

    public float stopCheckerboardRecording() {
        try {
            List<Float> checkerboard = (List<Float>)mStopCheckerboardRecording.invoke(null);
            float total = 0;
            for (float val : checkerboard) {
                total += val;
            }
            return total * 100.0f;
        } catch (IllegalAccessException e) {
            log(LogLevel.ERROR, e);
        } catch (InvocationTargetException e) {
            log(LogLevel.ERROR, e);
        }

        return 0.0f;
    }

    private View getSurfaceView() {
        ArrayList<View> views = mSolo.getCurrentViews();
        try {
            Class c = Class.forName("org.mozilla.gecko.gfx.LayerView");
            for (View v : views) {
                if (c.isInstance(v)) {
                    return v;
                }
            }
        } catch (ClassNotFoundException e) {
            log(LogLevel.ERROR, e);
        }
        log(LogLevel.WARN, "getSurfaceView could not find LayerView");
        for (View v : views) {
            log(LogLevel.WARN, v.toString());
        }
        return null;
    }

    public PaintedSurface getPaintedSurface() {
        View view = getSurfaceView();
        if (view == null) {
            return null;
        }
        IntBuffer pixelBuffer;
        try {
            pixelBuffer = (IntBuffer)mGetPixels.invoke(mRobocopApi, view);
        } catch (Exception e) {
            log(LogLevel.ERROR, e);
            return null;
        }

        // now we need to (1) flip the image, because GL likes to do things up-side-down,
        // and (2) rearrange the bits from AGBR-8888 to ARGB-8888.
        int w = view.getWidth();
        int h = view.getHeight();
        pixelBuffer.position(0);
        String mapFile = mRootPath + "/pixels.map";

        FileOutputStream fos = null;
        BufferedOutputStream bos = null;
        DataOutputStream dos = null;
        try {
            fos = new FileOutputStream(mapFile);
            bos = new BufferedOutputStream(fos);
            dos = new DataOutputStream(bos);

            for (int y = h - 1; y >= 0; y--) {
                for (int x = 0; x < w; x++) {
                    int agbr = pixelBuffer.get();
                    dos.writeInt((agbr & 0xFF00FF00) | ((agbr >> 16) & 0x000000FF) | ((agbr << 16) & 0x00FF0000));
                }
            }
        } catch (IOException e) {
            throw new RoboCopException("exception with pixel writer on file: " + mapFile);
        } finally {
            try {
                if (dos != null) {
                    dos.flush();
                    dos.close();
                }
                // closing dos automatically closes bos
                if (fos != null) {
                    fos.flush();
                    fos.close();
                }
            } catch (IOException e) {
                log(LogLevel.ERROR, e);
                throw new RoboCopException("exception closing pixel writer on file: " + mapFile);
            }
        }
        return new PaintedSurface(mapFile, w, h);
    }

    public int mHeight=0;
    public int mScrollHeight=0;
    public int mPageHeight=10;

    class scrollHandler implements InvocationHandler {
        public scrollHandler(){};
        public Object invoke(Object proxy, Method method, Object[] args) {
            try {
                // Disect the JSON object into the appropriate variables 
                JSONObject jo = ((JSONObject)args[1]);
                mScrollHeight = jo.getInt("y");
                mHeight = jo.getInt("cheight");
                // We don't want a height of 0. That means it's a bad response.
                if (mHeight > 0) {
                    mPageHeight = jo.getInt("height");
                }

            } catch( Throwable e) {
                FennecNativeDriver.log(FennecNativeDriver.LogLevel.WARN, 
                    "WARNING: ScrollReceived, but read wrong!");
            }
            return null;
        }
    }

    public int getScrollHeight() {
        return mScrollHeight;
    }
    public int getPageHeight() {
        return mPageHeight;
    }
    public int getHeight() {
        return mHeight;
    }

    public void setupScrollHandling() {
        //Setup scrollHandler to catch "robocop:scroll" events. 
        try {
            Class [] interfaces = new Class[1];
            interfaces[0] = mEventListenerClass;
            Object[] finalParams = new Object[2];
            finalParams[0] = "robocop:scroll";
            finalParams[1] = Proxy.newProxyInstance(mClassLoader, interfaces, new scrollHandler());
            mRegisterEventListener.invoke(mRobocopApi, finalParams);
        } catch (IllegalAccessException e) {
            log(LogLevel.ERROR, e);
        } catch (InvocationTargetException e) {
            log(LogLevel.ERROR, e);
        }

    }

    /**
     *  Takes a filename, loads the file, and returns a string version of the entire file.
     */
    public static String getFile(String filename)
    {
        StringBuilder text = new StringBuilder();

        BufferedReader br = null;
        try {
            br = new BufferedReader(new FileReader(filename));
            String line;

            while ((line = br.readLine()) != null) {
                text.append(line);
                text.append('\n');
            }
        } catch (IOException e) {
            log(LogLevel.ERROR, e);
        } finally {
            try {
                br.close();
            } catch (IOException e) {
            }
        }
        return text.toString();    
    }

    /**
     *  Takes a string of "key=value" pairs split by \n and creates a hash table.
     */
    public static HashMap convertTextToTable(String data)
    {
        HashMap retVal = new HashMap();

        String[] lines = data.split("\n");
        for (int i = 0; i < lines.length; i++) {
            String[] parts = lines[i].split("=", 2);
            retVal.put(parts[0].trim(), parts[1].trim());
        }
        return retVal;
    }

    public static void logAllStackTraces(LogLevel level) {
        StringBuffer sb = new StringBuffer();
        sb.append("Dumping ALL the threads!\n");
        Map<Thread, StackTraceElement[]> allStacks = Thread.getAllStackTraces();
        for (Thread t : allStacks.keySet()) {
            sb.append(t.toString()).append('\n');
            for (StackTraceElement ste : allStacks.get(t)) {
                sb.append(ste.toString()).append('\n');
            }
            sb.append('\n');
        }
        log(level, sb.toString());
    }

    /** 
     *  Set the filename used for logging. If the file already exists, delete it
     *  as a safe-guard against accidentally appending to an old log file.
     */
    public static void setLogFile(String filename) {
        mLogFile = filename;
        File file = new File(mLogFile);
        if (file.exists()) {
            file.delete();
        }
    }

    public static void setLogLevel(LogLevel level) {
        mLogLevel = level;
    }

    public static void log(LogLevel level, String message) {
        log(level, message, null);
    }

    public static void log(LogLevel level, Throwable t) {
        log(level, null, t);
    }

    public static void log(LogLevel level, String message, Throwable t) {
        if (mLogFile == null) {
            assert(false);
        }

        if (level.isEnabled(mLogLevel)) {
            PrintWriter pw = null;
            try {
                pw = new PrintWriter(new FileWriter(mLogFile, true));
                if (message != null) {
                    pw.println(message);
                }
                if (t != null) {
                    t.printStackTrace(pw);
                }
            } catch (IOException ioe) {
                Log.e("Robocop", "exception with file writer on: " + mLogFile);
            } finally {
                pw.close();
            }
            // PrintWriter doesn't throw IOE but sets an error flag instead,
            // so check for that
            if (pw.checkError()) {
                Log.e("Robocop", "exception with file writer on: " + mLogFile);
            }
        }

        if (level == LogLevel.INFO) {
            Log.i("Robocop", message, t);
        } else if (level == LogLevel.DEBUG) {
            Log.d("Robocop", message, t);
        } else if (level == LogLevel.WARN) {
            Log.w("Robocop", message, t);
        } else if (level == LogLevel.ERROR) {
            Log.e("Robocop", message, t);
        }
    }
}
