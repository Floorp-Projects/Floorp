/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Vivien Nicolas <vnicolas@mozilla.com>
 *   Sriram Ramasubramanian <sriram@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import java.io.*;
import java.util.*;
import java.util.zip.*;
import java.net.MalformedURLException;
import java.net.URL;
import java.nio.*;
import java.nio.channels.FileChannel;
import java.util.concurrent.*;
import java.lang.reflect.*;

import org.json.*;

import android.os.*;
import android.app.*;
import android.text.*;
import android.view.*;
import android.view.inputmethod.*;
import android.content.*;
import android.content.res.*;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.BitmapDrawable;
import android.widget.*;
import android.hardware.*;

import android.util.*;
import android.net.*;
import android.database.*;
import android.database.sqlite.*;
import android.provider.*;
import android.content.pm.*;
import android.content.pm.PackageManager.*;
import android.content.SharedPreferences.*;
import dalvik.system.*;

abstract public class GeckoApp
    extends Activity implements GeckoEventListener 
{
    private static final String LOG_FILE_NAME     = "GeckoApp";

    public static final String ACTION_ALERT_CLICK = "org.mozilla.gecko.ACTION_ALERT_CLICK";
    public static final String ACTION_ALERT_CLEAR = "org.mozilla.gecko.ACTION_ALERT_CLEAR";
    public static final String ACTION_WEBAPP      = "org.mozilla.gecko.WEBAPP";
    public static final String ACTION_DEBUG       = "org.mozilla.gecko.DEBUG";
    public static final String ACTION_BOOKMARK    = "org.mozilla.gecko.BOOKMARK";

    private LinearLayout mMainLayout;
    private RelativeLayout mGeckoLayout;
    public static GeckoSurfaceView surfaceView;
    public static SurfaceView cameraView;
    public static GeckoApp mAppContext;
    public static boolean mFullscreen = false;
    public static File sGREDir = null;
    public static Menu sMenu;
    public Handler mMainHandler;
    private IntentFilter mConnectivityFilter;
    private BroadcastReceiver mConnectivityReceiver;
    private BrowserToolbar mBrowserToolbar;
    private PopupWindow mTabsTray;
    private TabsAdapter mTabsAdapter;
    public DoorHanger mDoorHanger;
    private static boolean sIsTabsTrayShowing;
    private static boolean sIsGeckoReady = false;

    static class ExtraMenuItem implements MenuItem.OnMenuItemClickListener {
        String label;
        String icon;
        int id;
        public boolean onMenuItemClick(MenuItem item) {
            Log.i("GeckoJSMenu", "menu item clicked");
            GeckoAppShell.sendEventToGecko(new GeckoEvent("Menu:Clicked", Integer.toString(id)));
            return true;
        }
    }

    static Vector<ExtraMenuItem> sExtraMenuItems = new Vector<ExtraMenuItem>();

    enum LaunchState {Launching, WaitButton,
                      Launched, GeckoRunning, GeckoExiting};
    private static LaunchState sLaunchState = LaunchState.Launching;
    private static boolean sTryCatchAttached = false;

    private static final int FILE_PICKER_REQUEST = 1;
    private static final int AWESOMEBAR_REQUEST = 2;
    private static final int CAMERA_CAPTURE_REQUEST = 3;

    static boolean checkLaunchState(LaunchState checkState) {
        synchronized(sLaunchState) {
            return sLaunchState == checkState;
        }
    }

    static void setLaunchState(LaunchState setState) {
        synchronized(sLaunchState) {
            sLaunchState = setState;
        }
    }

    // if mLaunchState is equal to checkState this sets mLaunchState to setState
    // and return true. Otherwise we return false.
    static boolean checkAndSetLaunchState(LaunchState checkState, LaunchState setState) {
        synchronized(sLaunchState) {
            if (sLaunchState != checkState)
                return false;
            sLaunchState = setState;
            return true;
        }
    }

    void showErrorDialog(String message)
    {
        new AlertDialog.Builder(this)
            .setMessage(message)
            .setCancelable(false)
            .setPositiveButton(R.string.exit_label,
                               new DialogInterface.OnClickListener() {
                                   public void onClick(DialogInterface dialog,
                                                       int id)
                                   {
                                       GeckoApp.this.finish();
                                       System.exit(0);
                                   }
                               }).show();
    }

    public static final String PLUGIN_ACTION = "android.webkit.PLUGIN";

    /**
     * A plugin that wish to be loaded in the WebView must provide this permission
     * in their AndroidManifest.xml.
     */
    public static final String PLUGIN_PERMISSION = "android.webkit.permission.PLUGIN";

    private static final String LOGTAG = "PluginManager";

    private static final String PLUGIN_SYSTEM_LIB = "/system/lib/plugins/";

    private static final String PLUGIN_TYPE = "type";
    private static final String TYPE_NATIVE = "native";
    public ArrayList<PackageInfo> mPackageInfoCache = new ArrayList<PackageInfo>();

    String[] getPluginDirectories() {

        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - start of getPluginDirectories");

        ArrayList<String> directories = new ArrayList<String>();
        PackageManager pm = this.mAppContext.getPackageManager();
        List<ResolveInfo> plugins = pm.queryIntentServices(new Intent(PLUGIN_ACTION),
                PackageManager.GET_SERVICES | PackageManager.GET_META_DATA);

        synchronized(mPackageInfoCache) {

            // clear the list of existing packageInfo objects
            mPackageInfoCache.clear();


            for (ResolveInfo info : plugins) {

                // retrieve the plugin's service information
                ServiceInfo serviceInfo = info.serviceInfo;
                if (serviceInfo == null) {
                    Log.w(LOGTAG, "Ignore bad plugin");
                    continue;
                }

                Log.w(LOGTAG, "Loading plugin: " + serviceInfo.packageName);


                // retrieve information from the plugin's manifest
                PackageInfo pkgInfo;
                try {
                    pkgInfo = pm.getPackageInfo(serviceInfo.packageName,
                                    PackageManager.GET_PERMISSIONS
                                    | PackageManager.GET_SIGNATURES);
                } catch (Exception e) {
                    Log.w(LOGTAG, "Can't find plugin: " + serviceInfo.packageName);
                    continue;
                }
                if (pkgInfo == null) {
                    Log.w(LOGTAG, "Loading plugin: " + serviceInfo.packageName + ". Could not load package information.");
                    continue;
                }

                /*
                 * find the location of the plugin's shared library. The default
                 * is to assume the app is either a user installed app or an
                 * updated system app. In both of these cases the library is
                 * stored in the app's data directory.
                 */
                String directory = pkgInfo.applicationInfo.dataDir + "/lib";
                final int appFlags = pkgInfo.applicationInfo.flags;
                final int updatedSystemFlags = ApplicationInfo.FLAG_SYSTEM |
                                               ApplicationInfo.FLAG_UPDATED_SYSTEM_APP;
                // preloaded system app with no user updates
                if ((appFlags & updatedSystemFlags) == ApplicationInfo.FLAG_SYSTEM) {
                    directory = PLUGIN_SYSTEM_LIB + pkgInfo.packageName;
                }

                // check if the plugin has the required permissions
                String permissions[] = pkgInfo.requestedPermissions;
                if (permissions == null) {
                    Log.w(LOGTAG, "Loading plugin: " + serviceInfo.packageName + ". Does not have required permission.");
                    continue;
                }
                boolean permissionOk = false;
                for (String permit : permissions) {
                    if (PLUGIN_PERMISSION.equals(permit)) {
                        permissionOk = true;
                        break;
                    }
                }
                if (!permissionOk) {
                    Log.w(LOGTAG, "Loading plugin: " + serviceInfo.packageName + ". Does not have required permission (2).");
                    continue;
                }

                // check to ensure the plugin is properly signed
                Signature signatures[] = pkgInfo.signatures;
                if (signatures == null) {
                    Log.w(LOGTAG, "Loading plugin: " + serviceInfo.packageName + ". Not signed.");
                    continue;
                }

                // determine the type of plugin from the manifest
                if (serviceInfo.metaData == null) {
                    Log.e(LOGTAG, "The plugin '" + serviceInfo.name + "' has no type defined");
                    continue;
                }

                String pluginType = serviceInfo.metaData.getString(PLUGIN_TYPE);
                if (!TYPE_NATIVE.equals(pluginType)) {
                    Log.e(LOGTAG, "Unrecognized plugin type: " + pluginType);
                    continue;
                }

                try {
                    Class<?> cls = getPluginClass(serviceInfo.packageName, serviceInfo.name);

                    //TODO implement any requirements of the plugin class here!
                    boolean classFound = true;

                    if (!classFound) {
                        Log.e(LOGTAG, "The plugin's class' " + serviceInfo.name + "' does not extend the appropriate class.");
                        continue;
                    }

                } catch (NameNotFoundException e) {
                    Log.e(LOGTAG, "Can't find plugin: " + serviceInfo.packageName);
                    continue;
                } catch (ClassNotFoundException e) {
                    Log.e(LOGTAG, "Can't find plugin's class: " + serviceInfo.name);
                    continue;
                }

                // if all checks have passed then make the plugin available
                mPackageInfoCache.add(pkgInfo);
                directories.add(directory);
            }
        }

        String [] result = directories.toArray(new String[directories.size()]);
        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - end of getPluginDirectories");
        return result;
    }

    Class<?> getPluginClass(String packageName, String className)
            throws NameNotFoundException, ClassNotFoundException {
        Context pluginContext = this.mAppContext.createPackageContext(packageName,
                Context.CONTEXT_INCLUDE_CODE |
                Context.CONTEXT_IGNORE_SECURITY);
        ClassLoader pluginCL = pluginContext.getClassLoader();
        return pluginCL.loadClass(className);
    }

    // Returns true when the intent is going to be handled by gecko launch
    boolean launch(Intent intent)
    {
        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - launch");
        
        if (!checkAndSetLaunchState(LaunchState.Launching, LaunchState.Launched))
            return false;

        class GeckoTask extends AsyncTask<Intent, Void, Void> {
            protected Void doInBackground(Intent... intents) {
                Intent intent = intents[0];
                File cacheFile = GeckoAppShell.getCacheDir();
                File libxulFile = new File(cacheFile, "libxul.so");

                if ((!libxulFile.exists() ||
                     new File(getApplication().getPackageResourcePath()).lastModified() >= libxulFile.lastModified())) {
                    File[] libs = cacheFile.listFiles(new FilenameFilter() {
                            public boolean accept(File dir, String name) {
                                return name.endsWith(".so");
                            }
                        });
                    if (libs != null) {
                        for (int i = 0; i < libs.length; i++) {
                            libs[i].delete();
                        }
                    }
                 }
 
                // At some point while loading the gecko libs our default locale gets set
                // so just save it to locale here and reset it as default after the join
                Locale locale = Locale.getDefault();
                GeckoAppShell.loadGeckoLibs(
                    getApplication().getPackageResourcePath());
                Locale.setDefault(locale);
                Resources res = getBaseContext().getResources();
                Configuration config = res.getConfiguration();
                config.locale = locale;
                res.updateConfiguration(config, res.getDisplayMetrics());

                Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - runGecko");

                // and then fire us up
                try {
                    String env = intent.getStringExtra("env0");
                    String uri = intent.getDataString();
                    if (uri == null || uri.equals(""))
                        uri = getLastUri();
                    if (uri == null)
                        uri = "";

                    final String awesomeURI = uri; 
                    mMainHandler.post(new Runnable() {
                      public void run() {
                        mBrowserToolbar.setTitle(awesomeURI);
                      }
                    });

                    Log.w(LOGTAG, "RunGecko - URI = " + uri);

                    GeckoAppShell.runGecko(getApplication().getPackageResourcePath(),
                                           intent.getStringExtra("args"),
                                           uri);
                } catch (Exception e) {
                    Log.e(LOG_FILE_NAME, "top level exception", e);
                    StringWriter sw = new StringWriter();
                    e.printStackTrace(new PrintWriter(sw));
                    GeckoAppShell.reportJavaCrash(sw.toString());
                }
                return null;
            }
        }

        if (intent == null)
            intent = getIntent();

        new GeckoTask().execute(intent);

        return true;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        sMenu = menu;
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.layout.gecko_menu, menu);
        if (sIsGeckoReady)
            menu.findItem(R.id.preferences).setEnabled(true);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu aMenu)
    {
        Iterator<ExtraMenuItem> i = sExtraMenuItems.iterator();
        while (i.hasNext()) {
            final ExtraMenuItem item = i.next();
            if (aMenu.findItem(item.id) == null) {
                final MenuItem mi = aMenu.add(aMenu.NONE, item.id, aMenu.NONE, item.label);
                if (item.icon != null) {
                    if (item.icon.startsWith("data")) {
                        byte[] raw = Base64.decode(item.icon.substring(22), Base64.DEFAULT);
                        Bitmap bitmap = BitmapFactory.decodeByteArray(raw, 0, raw.length);
                        BitmapDrawable drawable = new BitmapDrawable(bitmap);
                        mi.setIcon(drawable);
                    }
                    else if (item.icon.startsWith("jar:") || item.icon.startsWith("file://")) {
                        GeckoAppShell.getHandler().post(new Runnable() {
                            public void run() {
                                try {
                                    URL url = new URL(item.icon);
                                    InputStream is = (InputStream) url.getContent();
                                    Drawable drawable = Drawable.createFromStream(is, "src");
                                    mi.setIcon(drawable);
                                } catch(Exception e) {
                                    Log.e("Gecko", "onPrepareOptionsMenu: Unable to set icon", e);
                                }
                            }
                        });
                    }
                }
                mi.setOnMenuItemClickListener(item);
            }
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Tab tab = null;
        Tab.HistoryEntry he = null;
        switch (item.getItemId()) {
           case R.id.quit:
               quit();
               return true;
           case R.id.bookmarks:
               Intent intent = new Intent(this, GeckoBookmarks.class);
               tab = Tabs.getInstance().getSelectedTab();
               if (tab == null) {
                   startActivity(intent);
                   return true;
               }

               he = tab.getLastHistoryEntry();
               if (he != null) {
                   intent.setData(android.net.Uri.parse(he.mUri));
                   intent.putExtra("title", he.mTitle);
                   startActivity(intent);
               }
               return true;
           case R.id.share:
               tab = Tabs.getInstance().getSelectedTab();
               if (tab == null)
                   return true;

               he = tab.getLastHistoryEntry();
               if (he != null) {
                   GeckoAppShell.openUriExternal(he.mUri, "text/plain", "", "",
                                                 Intent.ACTION_SEND, he.mTitle);
               }
               return true;
           case R.id.reload:
               doReload();
               return true;
           case R.id.saveaspdf:
               GeckoAppShell.sendEventToGecko(new GeckoEvent("SaveAs:PDF", null));
               return true;
           case R.id.preferences:
               intent = new Intent(this, GeckoPreferences.class);
               startActivity(intent);
               return true;
           default:
               return super.onOptionsItemSelected(item);
        }
    }

    private void rememberLastScreen(boolean sync) {
        if (surfaceView == null)
            return;
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null)
            return;

        if (!tab.getHistory().empty()) {
            SharedPreferences prefs = getSharedPreferences("GeckoApp", 0);
            Editor editor = prefs.edit();
            
            String uri = tab.getHistory().peek().mUri;
            String title = tab.getHistory().peek().mTitle;

            editor.putString("last-uri", uri);
            editor.putString("last-title", title);

            Log.i(LOG_FILE_NAME, "Saving:: " + uri + " " + title);
            editor.commit();
            surfaceView.saveLast(sync);
        }
    }

    private String getLastUri() {
        SharedPreferences prefs = getSharedPreferences("GeckoApp", 0);
        String lastUri = prefs.getString("last-uri", "");
        return lastUri;
    }

    private boolean restoreLastScreen() {
        SharedPreferences prefs = getSharedPreferences ("GeckoApp", 0);
        String lastUri = prefs.getString("last-uri", "");
        String lastTitle = prefs.getString("last-title", "");

        Log.i(LOG_FILE_NAME, "The last uri was: " + lastUri);
        Log.i(LOG_FILE_NAME, "The last title was: " + lastTitle);
        
        return true;
    }

    private void quit() {
        Log.i(LOG_FILE_NAME, "pleaseKillMe");
        rememberLastScreen(true);
        System.exit(0);
    }

    void handleLocationChange(final int tabId, final String uri) {
        Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;
        
        String oldBaseURI = tab.getURL();
        tab.updateURL(uri);

        if (!Tabs.getInstance().isSelectedTab(tab))
            return;

        String baseURI = uri;
        if (baseURI.indexOf('#') != -1)
            baseURI = uri.substring(0, uri.indexOf('#'));

        if (oldBaseURI != null && oldBaseURI.indexOf('#') != -1)
            oldBaseURI = oldBaseURI.substring(0, oldBaseURI.indexOf('#'));
        
        if (baseURI.equals(oldBaseURI))
            return;

        mMainHandler.post(new Runnable() { 
            public void run() {
                mBrowserToolbar.setTitle(uri);
                mBrowserToolbar.setFavicon(null);
            }
        });
    }

    void showTabs() {
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        
        int width = metrics.widthPixels;
        int height = (int) (metrics.widthPixels * 0.75);
        LayoutInflater inflater = (LayoutInflater) mAppContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mTabsTray = new PopupWindow(inflater.inflate(R.layout.tabs_tray, null, false),
                            width,
                            height,
                            true);
        mTabsTray.setBackgroundDrawable(new BitmapDrawable());
        mTabsTray.setOutsideTouchable(true);
        
        ListView list = (ListView) mTabsTray.getContentView().findViewById(R.id.list);
        Button addTab = new Button(this);
        addTab.setText(R.string.new_tab);
        addTab.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                hideTabs();
                Intent intent = new Intent(mAppContext, AwesomeBar.class);
                intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION | Intent.FLAG_ACTIVITY_NO_HISTORY);
                intent.putExtra(AwesomeBar.TYPE_KEY, AwesomeBar.Type.ADD.name());
                startActivityForResult(intent, AWESOMEBAR_REQUEST);
            }
        });

        list.addFooterView(addTab);
        sIsTabsTrayShowing = true;
        onTabsChanged();
        mTabsTray.showAsDropDown(mBrowserToolbar.findViewById(R.id.tabs));
    }
    
    void hideTabs() {
        if (mTabsTray.isShowing()) {
            mTabsAdapter = null;
            ((ListView) mTabsTray.getContentView().findViewById(R.id.list)).invalidateViews();
            mTabsTray.dismiss();
            sIsTabsTrayShowing = false;
        }
    }

    public void onTabsChanged() {
        if (mTabsTray == null)
            return;

        if (!sIsTabsTrayShowing)
            return;

        final HashMap<Integer, Tab> tabs = Tabs.getInstance().getTabs();
        if (mTabsAdapter != null) {
            mTabsAdapter = new TabsAdapter(mAppContext, tabs);
            mTabsAdapter.notifyDataSetChanged();
            ListView list = (ListView) mTabsTray.getContentView().findViewById(R.id.list);
            list.invalidateViews();
            list.setAdapter(mTabsAdapter);
        } else {
            mTabsAdapter = new TabsAdapter(mAppContext, tabs);
            ListView list = (ListView) mTabsTray.getContentView().findViewById(R.id.list);
            list.setAdapter(mTabsAdapter);
        }
    }

    public void handleMessage(String event, JSONObject message) {
        Log.i("Gecko", "Got message: " + event);
        try {
            if (event.equals("Menu:Add")) {
                String name = message.getString("name");
                ExtraMenuItem item = new ExtraMenuItem();
                item.label = message.getString("name");
                item.id = message.getInt("id");
                try { // icon is optional
                    item.icon = message.getString("icon");
                } catch (Exception ex) { }
                sExtraMenuItems.add(item);
            } else if (event.equals("Menu:Remove")) {
                // remove it from the menu and from our vector
                Iterator<ExtraMenuItem> i = sExtraMenuItems.iterator();
                int id = message.getInt("id");
                while (i.hasNext()) {
                    ExtraMenuItem item = i.next();
                    if (item.id == id) {
                        sExtraMenuItems.remove(item);
                        MenuItem menu = sMenu.findItem(id);
                        if (menu != null)
                            sMenu.removeItem(id);
                        return;
                    }
                }
            } else if (event.equals("Toast:Show")) {
                final String msg = message.getString("message");
                final String duration = message.getString("duration");
                handleShowToast(msg, duration);
            } else if (event.equals("DOMContentLoaded")) {
                final int tabId = message.getInt("tabID");
                final String uri = message.getString("uri");
                final String title = message.getString("title");
                final CharSequence titleText = title;
                handleContentLoaded(tabId, uri, title);
                Log.i(LOG_FILE_NAME, "URI - " + uri + ", title - " + title);
            } else if (event.equals("DOMTitleChanged")) {
                final int tabId = message.getInt("tabID");
                final String title = message.getString("title");
                final CharSequence titleText = title;
                handleTitleChanged(tabId, title);
                Log.i(LOG_FILE_NAME, "title - " + title);
            } else if (event.equals("DOMLinkAdded")) {
                final int tabId = message.getInt("tabID");
                final String rel = message.getString("rel");
                final String href = message.getString("href");
                Log.i(LOG_FILE_NAME, "link rel - " + rel + ", href - " + href);
                handleLinkAdded(tabId, rel, href);
            } else if (event.equals("log")) {
                // generic log listener
                final String msg = message.getString("msg");
                Log.i(LOG_FILE_NAME, "Log: " + msg);
            } else if (event.equals("onLocationChange")) {
                final int tabId = message.getInt("tabID");
                final String uri = message.getString("uri");
                Log.i(LOG_FILE_NAME, "URI - " + uri);
                handleLocationChange(tabId, uri);
            } else if (event.equals("onStateChange")) {
                final int tabId = message.getInt("tabID");
                int state = message.getInt("state");
                Log.i(LOG_FILE_NAME, "State - " + state);
                if ((state & GeckoAppShell.WPL_STATE_IS_DOCUMENT) != 0) {
                    if ((state & GeckoAppShell.WPL_STATE_START) != 0) {
                        Log.i(LOG_FILE_NAME, "Got a document start");
                        handleDocumentStart(tabId);
                    } else if ((state & GeckoAppShell.WPL_STATE_STOP) != 0) {
                        Log.i(LOG_FILE_NAME, "Got a document stop");
                        handleDocumentStop(tabId);
                    }
                }
            } else if (event.equals("onProgressChange")) {
                final int tabId = message.getInt("tabID");
                final int current = message.getInt("current");
                final int total = message.getInt("total");
            
                handleProgressChange(tabId, current, total);
                Log.i(LOG_FILE_NAME, "progress - " + current + "/" + total);
            } else if (event.equals("onCameraCapture")) {
                //GeckoApp.mAppContext.doCameraCapture(message.getString("path"));
                doCameraCapture();
            } else if (event.equals("Tab:Added")) {
                Log.i(LOG_FILE_NAME, "Created a new tab");
                int tabId = message.getInt("tabID");
                String uri = message.getString("uri");
                handleAddTab(tabId, uri);
            } else if (event.equals("Tab:Closed")) {
                Log.i(LOG_FILE_NAME, "Destroyed a tab");
                int tabId = message.getInt("tabID");
                handleCloseTab(tabId);
            } else if (event.equals("Tab:Selected")) {
                int tabId = message.getInt("tabID");
                Log.i(LOG_FILE_NAME, "Switched to tab: " + tabId);
                handleSelectTab(tabId);
            } else if (event.equals("Doorhanger:Add")) {
                int tabId = message.getInt("tabID");
                handleDoorHanger(message, tabId);
            } else if (event.equals("Preferences:Data")) {
                JSONArray jsonPrefs = message.getJSONArray("preferences");
                GeckoPreferences.refresh(jsonPrefs);
            } else if (event.equals("Gecko:Ready")) {
                sIsGeckoReady = true;
                sMenu.findItem(R.id.preferences).setEnabled(true);
            }
        } catch (Exception e) { 
            Log.i(LOG_FILE_NAME, "handleMessage throws " + e + " for message: " + event);
        }
    }

    void handleDoorHanger(JSONObject geckoObject, final int tabId) throws JSONException {
        final String msg = geckoObject.getString("message");
        Log.i(LOG_FILE_NAME, "DoorHanger received for tab " + tabId
              + ", msg:" + msg);
        final JSONArray buttons = geckoObject.getJSONArray("buttons");

        mMainHandler.post(new Runnable() {
                public void run() {
                    DoorHangerPopup dhp =
                        mAppContext.mDoorHanger.getPopup();
                    dhp.setTab(tabId);
                    for (int i = 0; i < buttons.length(); i++) {
                        JSONObject jo;
                        String label;
                        int callBackId;
                        try {
                            jo = buttons.getJSONObject(i);
                            label = jo.getString("label");
                            callBackId = jo.getInt("callback");
                            Log.i(LOG_FILE_NAME, "Label: " + label
                                  + " CallbackId: " + callBackId);
                            dhp.addButton(label, callBackId);
                        } catch (JSONException e) {
                            Log.i(LOG_FILE_NAME, "JSON throws " + e);
                        }
                    }
                    dhp.setText(msg);

                    // Show doorhanger if it is on the active tab
                    int activeTab = Tabs.getInstance().getSelectedTabId();
                    mAppContext.mDoorHanger.updateForTab(activeTab);
                }
           });
    }

    void handleAddTab(final int tabId, final String uri) {
        Tab tab = Tabs.getInstance().addTab(tabId, uri);
        mMainHandler.post(new Runnable() { 
            public void run() {
                onTabsChanged();
                mBrowserToolbar.updateTabs(Tabs.getInstance().getCount());
                mDoorHanger.updateForTab(tabId);
            }
        });
    }
    
    void handleCloseTab(final int tabId) {
        Tabs.getInstance().removeTab(tabId);
        
        mMainHandler.post(new Runnable() { 
            public void run() {
                onTabsChanged();
                mBrowserToolbar.updateTabs(Tabs.getInstance().getCount());
                mDoorHanger.removeForTab(tabId);
            }
        });
    }
    
    void handleSelectTab(final int tabId) {
        final Tab tab = Tabs.getInstance().selectTab(tabId);
        if (tab == null)
            return;

        mMainHandler.post(new Runnable() { 
            public void run() {
                mBrowserToolbar.setTitle(tab.getTitle());
                mBrowserToolbar.setFavicon(tab.getFavicon());
                mBrowserToolbar.setProgressVisibility(tab.isLoading());
                mDoorHanger.updateForTab(tabId);
            }
        });
    }

    void handleDocumentStart(final int tabId) {
        Tab tab = Tabs.getInstance().getTab(tabId);

        if (tab == null)
            return;

        tab.setLoading(true);
        
        mMainHandler.post(new Runnable() {
            public void run() {
                onTabsChanged();
            }
        });

        if (!Tabs.getInstance().isSelectedTab(tab))
            return;

        mMainHandler.post(new Runnable() { 
            public void run() {
                mBrowserToolbar.setProgressVisibility(true);
                mBrowserToolbar.updateProgress(-1, -1);
            }
        });
    }

    void handleDocumentStop(final int tabId) {
        Tab tab = Tabs.getInstance().getTab(tabId);

        if (tab == null)
            return;

         tab.setLoading(false);
         
        mMainHandler.post(new Runnable() {
            public void run() {
                onTabsChanged();
            }
        });
        
        if (!Tabs.getInstance().isSelectedTab(tab))
            return;

        mMainHandler.post(new Runnable() { 
            public void run() {
                mBrowserToolbar.setProgressVisibility(false);
                surfaceView.hideStartupBitmap();
            }
        });
    }

    void handleProgressChange(final int tabId, final int current, final int total) {
        Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        if (!Tabs.getInstance().isSelectedTab(tab))
            return;
        
        mMainHandler.post(new Runnable() { 
            public void run() {
                mBrowserToolbar.updateProgress(current, total);
            }
        });
    }

    void handleShowToast(final String message, final String duration) {
        mMainHandler.post(new Runnable() {
            public void run() {
                Toast toast;
                if (duration.equals("long"))
                    toast = Toast.makeText(mAppContext, message, Toast.LENGTH_LONG);
                else
                    toast = Toast.makeText(mAppContext, message, Toast.LENGTH_SHORT);
                toast.show();
            }
        });
    }

    void handleContentLoaded(final int tabId, final String uri, final String title) {
        Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.updateTitle(title);
        tab.addHistory(new Tab.HistoryEntry(uri, title));

        mMainHandler.post(new Runnable() {
            public void run() {
                onTabsChanged();
            }
        });

        if (tab.getFavicon() == null)
            downloadDefaultFavicon(tabId);

        if (!Tabs.getInstance().isSelectedTab(tab))
            return;

        mMainHandler.post(new Runnable() {
            public void run() {
                mBrowserToolbar.setTitle(title);
            }
        });
    }

    void handleTitleChanged(final int tabId, final String title) {
        Tab tab = Tabs.getInstance().getTab(tabId);
        if (tab == null)
            return;

        tab.updateTitle(title);
        
        if (!Tabs.getInstance().isSelectedTab(tab))
            return;

        mMainHandler.post(new Runnable() { 
            public void run() {
                onTabsChanged();
                mBrowserToolbar.setTitle(title);
            }
        });
    }

    void handleLinkAdded(final int tabId, String rel, final String href) {
        if (rel.indexOf("icon") != -1) {
            new DownloadFaviconTask().execute(href, "" + tabId);
        }
    }

    void downloadDefaultFavicon(final int tabId) {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null)
            return;

        try {
            URL url = new URL(tab.getURL());
            String faviconUrl = url.getProtocol() + "://" + url.getAuthority() + "/favicon.ico";
            new DownloadFaviconTask().execute(faviconUrl, "" + tabId);
        } catch (MalformedURLException e) {
            // Optional so not a real error
        }
    }

    private class DownloadFaviconTask extends AsyncTask<String, Void, Drawable> {
        private int tabId;

        protected Drawable doInBackground(String... args) {
            Drawable image = null;
            
            try {
                URL url = new URL(args[0]);
                tabId = Integer.parseInt(args[1]); 

                InputStream is = (InputStream) url.getContent();
                image = Drawable.createFromStream(is, "src");
            } catch (IOException e) {
                Log.d(LOG_FILE_NAME, "Error loading favicon: " + e);
            }

            return image;
        }

        protected void onPostExecute(Drawable image) {
            if (image != null) {
                Tab tab = Tabs.getInstance().getTab(tabId);
                if (tab == null)
                    return;

                tab.updateFavicon(image);

                mMainHandler.post(new Runnable() {
                    public void run() {
                        onTabsChanged();
                    }
                });

                if (!Tabs.getInstance().isSelectedTab(tab))
                    return;

                final Drawable postImage = image;
                mMainHandler.post(new Runnable() {
                    public void run() {
                        mBrowserToolbar.setFavicon(postImage);
                    }
                });
            }
        }
    }
    
    void addPluginView(final View view,
                       final double x, final double y,
                       final double w, final double h) {
        mMainHandler.post(new Runnable() { 
            public void run() {
                RelativeLayout.LayoutParams lp = new RelativeLayout.LayoutParams((int) w, (int) h);
                lp.leftMargin = (int) x;
                lp.topMargin = (int) y;

                if (mGeckoLayout.indexOfChild(view) == -1) {
                    view.setWillNotDraw(false);
                    if (view instanceof SurfaceView)
                        ((SurfaceView)view).setZOrderOnTop(true);

                    mGeckoLayout.addView(view, lp);
                } else {
                    try {
                        mGeckoLayout.updateViewLayout(view, lp);
                    } catch (IllegalArgumentException e) {
                        Log.i("updateViewLayout - IllegalArgumentException", "e:" + e);
                        // it can be the case where we
                        // get an update before the view
                        // is actually attached.
                    }
                }
            }
        });
    }

    void removePluginView(final View view) {
        mMainHandler.post(new Runnable() { 
            public void run() {
                try {
                    mGeckoLayout.removeView(view);
                } catch (Exception e) {}
            }
        });
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - onCreate");

        if (Build.VERSION.SDK_INT >= 9) {
            StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                                       .detectDiskReads().detectDiskWrites().detectNetwork()
                                       .penaltyLog().build());
            StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder().penaltyLog().build());
        }
        if (Build.VERSION.SDK_INT >= 11) {
            setTheme(R.style.HoneycombTheme);
        }

        super.onCreate(savedInstanceState);
        
        getWindow().setFlags(mFullscreen ?
                             WindowManager.LayoutParams.FLAG_FULLSCREEN : 0,
                             WindowManager.LayoutParams.FLAG_FULLSCREEN);
        
        setContentView(R.layout.gecko_app);
        mAppContext = this;

        // setup gecko layout
        mGeckoLayout = (RelativeLayout) findViewById(R.id.gecko_layout);
        mMainLayout = (LinearLayout) findViewById(R.id.main_layout);
        mBrowserToolbar = (BrowserToolbar) findViewById(R.id.browser_toolbar);

        mDoorHanger = new DoorHanger(this);

        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            mBrowserToolbar.setTitle(tab.getTitle());
            mBrowserToolbar.setFavicon(tab.getFavicon());
            mBrowserToolbar.updateTabs(Tabs.getInstance().getCount()); 
        } 

        if (surfaceView == null) {
            surfaceView = new GeckoSurfaceView(this);
            mGeckoLayout.addView(surfaceView);
        } else if (mGeckoLayout.getChildCount() == 0) {
           //surfaceView still holds to the old one during rotation. re-add it to new activity
           ((ViewGroup) surfaceView.getParent()).removeAllViews();
           mGeckoLayout.addView(surfaceView);
        }

        surfaceView.loadStartupBitmap();

        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - UI almost up");

        if (sGREDir == null)
            sGREDir = new File(this.getApplicationInfo().dataDir);

        mMainHandler = new Handler();

        if (!sTryCatchAttached) {
            sTryCatchAttached = true;
            mMainHandler.post(new Runnable() {
                public void run() {
                    try {
                        Looper.loop();
                    } catch (Exception e) {
                        Log.e(LOG_FILE_NAME, "top level exception", e);
                        StringWriter sw = new StringWriter();
                        e.printStackTrace(new PrintWriter(sw));
                        GeckoAppShell.reportJavaCrash(sw.toString());
                    }
                    // resetting this is kinda pointless, but oh well
                    sTryCatchAttached = false;
                }
            });
        }

        //register for events
        GeckoAppShell.registerGeckoEventListener("DOMContentLoaded", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("DOMTitleChanged", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("DOMLinkAdded", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("log", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("onLocationChange", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("onStateChange", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("onProgressChange", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("onCameraCapture", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Tab:Added", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Tab:Closed", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Tab:Selected", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Doorhanger:Add", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Menu:Add", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Menu:Remove", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Preferences:Data", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Gecko:Ready", GeckoApp.mAppContext);
        GeckoAppShell.registerGeckoEventListener("Toast:Show", GeckoApp.mAppContext);

        mConnectivityFilter = new IntentFilter();
        mConnectivityFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        mConnectivityReceiver = new GeckoConnectivityReceiver();

        final GeckoApp self = this;
 
        mMainHandler.postDelayed(new Runnable() {
            public void run() {

                Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - pre checkLaunchState");

                /*
                  XXXX see bug 635342
                   We want to disable this code if possible.  It is about 145ms in runtime
                SharedPreferences settings = getPreferences(Activity.MODE_PRIVATE);
                String localeCode = settings.getString(getPackageName() + ".locale", "");
                if (localeCode != null && localeCode.length() > 0)
                    GeckoAppShell.setSelectedLocale(localeCode);
                */

                if (!checkLaunchState(LaunchState.Launched)) {
                    return;
                }

                // it would be good only to do this if MOZ_UPDATER was defined 
                long startTime = new Date().getTime();
                checkAndLaunchUpdate();
                Log.w(LOGTAG, "checking for an update took " + (new Date().getTime() - startTime) + "ms");
            }
        }, 50);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - onNewIntent");

        if (checkLaunchState(LaunchState.GeckoExiting)) {
            // We're exiting and shouldn't try to do anything else just incase
            // we're hung for some reason we'll force the process to exit
            System.exit(0);
            return;
        }
        final String action = intent.getAction();
        if (ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.WaitButton)) {
            final Button launchButton = new Button(this);
            launchButton.setText("Launch"); // don't need to localize
            launchButton.setOnClickListener(new Button.OnClickListener() {
                public void onClick (View v) {
                    // hide the button so we can't be launched again
                    mMainLayout.removeView(launchButton);
                    setLaunchState(LaunchState.Launching);
                    launch(null);
                }
            });
            mMainLayout.addView(launchButton, 300, 200);
            return;
        }
        if (checkLaunchState(LaunchState.WaitButton) || launch(intent))
            return;

        if (Intent.ACTION_MAIN.equals(action)) {
            Log.i(LOG_FILE_NAME, "Intent : ACTION_MAIN");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(""));
        }
        else if (Intent.ACTION_VIEW.equals(action)) {
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(new GeckoEvent(uri));
            Log.i(LOG_FILE_NAME,"onNewIntent: "+uri);
        }
        else if (ACTION_WEBAPP.equals(action)) {
            String uri = intent.getStringExtra("args");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(uri));
            Log.i(LOG_FILE_NAME,"Intent : WEBAPP - " + uri);
        }
        else if (ACTION_BOOKMARK.equals(action)) {
            String args = intent.getStringExtra("args");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(args));
            Log.i(LOG_FILE_NAME,"Intent : BOOKMARK - " + args);
        }
    }

    @Override
    public void onPause()
    {
        Log.i(LOG_FILE_NAME, "pause");
        GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_PAUSING));
        // The user is navigating away from this activity, but nothing
        // has come to the foreground yet; for Gecko, we may want to
        // stop repainting, for example.

        // Whatever we do here should be fast, because we're blocking
        // the next activity from showing up until we finish.

        // onPause will be followed by either onResume or onStop.
        super.onPause();

        unregisterReceiver(mConnectivityReceiver);
    }

    @Override
    public void onResume()
    {
        Log.i(LOG_FILE_NAME, "resume");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.onResume();
        // After an onPause, the activity is back in the foreground.
        // Undo whatever we did in onPause.
        super.onResume();

        // Just in case. Normally we start in onNewIntent
        if (checkLaunchState(LaunchState.Launching))
            onNewIntent(getIntent());

        registerReceiver(mConnectivityReceiver, mConnectivityFilter);

        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                GeckoAppShell.getPromptService().onResume();
            }
        });
    }

    @Override
    public void onStop()
    {
        Log.i(LOG_FILE_NAME, "stop");
        // We're about to be stopped, potentially in preparation for
        // being destroyed.  We're killable after this point -- as I
        // understand it, in extreme cases the process can be terminated
        // without going through onDestroy.
        //
        // We might also get an onRestart after this; not sure what
        // that would mean for Gecko if we were to kill it here.
        // Instead, what we should do here is save prefs, session,
        // etc., and generally mark the profile as 'clean', and then
        // dirty it again if we get an onResume.


        GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_STOPPING));
        super.onStop();
    }

    @Override
    public void onRestart()
    {
        Log.i(LOG_FILE_NAME, "restart");
        super.onRestart();
    }

    @Override
    public void onStart()
    {
        Log.w(LOGTAG, "zerdatime " + new Date().getTime() + " - onStart");

        Log.i(LOG_FILE_NAME, "start");
        GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_START));
        super.onStart();
    }

    @Override
    public void onDestroy()
    {
        Log.i(LOG_FILE_NAME, "destroy");

        // Tell Gecko to shutting down; we'll end up calling System.exit()
        // in onXreExit.
        if (isFinishing())
            GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_SHUTDOWN));

        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                GeckoAppShell.getPromptService().onDestroy();
            }
        });
       
        if (mTabsTray != null && mTabsTray.isShowing()) {
            hideTabs();
            mTabsTray = null;
        }
        
        GeckoAppShell.unregisterGeckoEventListener("DOMContentLoaded", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("DOMTitleChanged", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("DOMLinkAdded", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("log", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("onLocationChange", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("onStateChange", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("onProgressChange", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("onCameraCapture", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Tab:Added", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Tab:Closed", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Tab:Selected", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Doorhanger:Add", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Menu:Add", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Menu:Remove", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Preferences:Data", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Gecko:Ready", GeckoApp.mAppContext);
        GeckoAppShell.unregisterGeckoEventListener("Toast:Show", GeckoApp.mAppContext);

        super.onDestroy();
    }

    @Override
    public void onConfigurationChanged(android.content.res.Configuration newConfig)
    {
        Log.i(LOG_FILE_NAME, "configuration changed");
        // nothing, just ignore
        super.onConfigurationChanged(newConfig);
    }

    @Override
    public void onLowMemory()
    {
        Log.e(LOG_FILE_NAME, "low memory");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.onLowMemory();
        super.onLowMemory();
    }

    abstract public String getPackageName();
    abstract public String getContentProcessName();

    public void addEnvToIntent(Intent intent) {
        Map<String,String> envMap = System.getenv();
        Set<Map.Entry<String,String>> envSet = envMap.entrySet();
        Iterator<Map.Entry<String,String>> envIter = envSet.iterator();
        StringBuffer envstr = new StringBuffer();
        int c = 0;
        while (envIter.hasNext()) {
            Map.Entry<String,String> entry = envIter.next();
            intent.putExtra("env" + c, entry.getKey() + "="
                            + entry.getValue());
            c++;
        }
    }

    public void doRestart() {
        try {
            String action = "org.mozilla.gecko.restart";
            Intent intent = new Intent(action);
            intent.setClassName(getPackageName(),
                                getPackageName() + ".Restarter");
            addEnvToIntent(intent);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK |
                            Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            Log.i(LOG_FILE_NAME, intent.toString());
            GeckoAppShell.killAnyZombies();
            startActivity(intent);
        } catch (Exception e) {
            Log.i(LOG_FILE_NAME, "error doing restart", e);
        }
        finish();
        // Give the restart process time to start before we die
        GeckoAppShell.waitForAnotherGeckoProc();
    }

    public void handleNotification(String action, String alertName, String alertCookie) {
        GeckoAppShell.handleNotification(action, alertName, alertCookie);
    }

    private void checkAndLaunchUpdate() {
        Log.i(LOG_FILE_NAME, "Checking for an update");

        int statusCode = 8; // UNEXPECTED_ERROR
        File baseUpdateDir = null;
        if (Build.VERSION.SDK_INT >= 8)
            baseUpdateDir = getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        else
            baseUpdateDir = new File(Environment.getExternalStorageDirectory().getPath(), "download");

        File updateDir = new File(new File(baseUpdateDir, "updates"),"0");

        File updateFile = new File(updateDir, "update.apk");
        File statusFile = new File(updateDir, "update.status");

        if (!statusFile.exists() || !readUpdateStatus(statusFile).equals("pending"))
            return;

        if (!updateFile.exists())
            return;

        Log.i(LOG_FILE_NAME, "Update is available!");

        // Launch APK
        File updateFileToRun = new File(updateDir, getPackageName() + "-update.apk");
        try {
            if (updateFile.renameTo(updateFileToRun)) {
                String amCmd = "/system/bin/am start -a android.intent.action.VIEW " +
                               "-n com.android.packageinstaller/.PackageInstallerActivity -d file://" +
                               updateFileToRun.getPath();
                Log.i(LOG_FILE_NAME, amCmd);
                Runtime.getRuntime().exec(amCmd);
                statusCode = 0; // OK
            } else {
                Log.i(LOG_FILE_NAME, "Cannot rename the update file!");
                statusCode = 7; // WRITE_ERROR
            }
        } catch (Exception e) {
            Log.i(LOG_FILE_NAME, "error launching installer to update", e);
        }

        // Update the status file
        String status = statusCode == 0 ? "succeeded\n" : "failed: "+ statusCode + "\n";

        OutputStream outStream;
        try {
            byte[] buf = status.getBytes("UTF-8");
            outStream = new FileOutputStream(statusFile);
            outStream.write(buf, 0, buf.length);
            outStream.close();
        } catch (Exception e) {
            Log.i(LOG_FILE_NAME, "error writing status file", e);
        }

        if (statusCode == 0)
            System.exit(0);
    }

    private String readUpdateStatus(File statusFile) {
        String status = "";
        try {
            BufferedReader reader = new BufferedReader(new FileReader(statusFile));
            status = reader.readLine();
            reader.close();
        } catch (Exception e) {
            Log.i(LOG_FILE_NAME, "error reading update status", e);
        }
        return status;
    }

    private SynchronousQueue<String> mFilePickerResult = new SynchronousQueue<String>();
    public String showFilePicker(String aMimeType) {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(aMimeType);
        GeckoApp.this.
            startActivityForResult(
                Intent.createChooser(intent, getString(R.string.choose_file)),
                FILE_PICKER_REQUEST);
        String filePickerResult = "";

        try {
            while (null == (filePickerResult = mFilePickerResult.poll(1, TimeUnit.MILLISECONDS))) {
                Log.i(LOG_FILE_NAME, "processing events from showFilePicker ");
                GeckoAppShell.processNextNativeEvent();
            }
        } catch (InterruptedException e) {
            Log.i(LOG_FILE_NAME, "showing file picker ",  e);
        }

        return filePickerResult;
    }

    @Override
    public boolean onSearchRequested() {
        return showAwesomebar(AwesomeBar.Type.ADD);
    }
 
    public boolean onEditRequested() {
        return showAwesomebar(AwesomeBar.Type.EDIT);
    }

    public boolean showAwesomebar(AwesomeBar.Type aType) {
        Intent intent = new Intent(getBaseContext(), AwesomeBar.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION | Intent.FLAG_ACTIVITY_NO_HISTORY);
        intent.putExtra(AwesomeBar.TYPE_KEY, aType.name());

        if (aType != AwesomeBar.Type.ADD) {
            // if we're not adding a new tab, show the old url
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null && !tab.getHistory().empty()) {
                intent.putExtra(AwesomeBar.CURRENT_URL_KEY, tab.getHistory().peek().mUri);
            }
        }
        startActivityForResult(intent, AWESOMEBAR_REQUEST);
        return true;
    }

    public boolean doReload() {
        Log.i(LOG_FILE_NAME, "Reload requested");
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null)
            return false;

        return tab.doReload();
    }

    @Override
    public void onBackPressed() {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null || !tab.doBack()) {
            finish();
        }
    }

    static int kCaptureIndex = 0;

    @Override
    protected void onActivityResult(int requestCode, int resultCode,
                                    Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch (requestCode) {
        case FILE_PICKER_REQUEST:
            String filePickerResult = "";
            if (data != null && resultCode == RESULT_OK) {
                try {
                    ContentResolver cr = getContentResolver();
                    Uri uri = data.getData();
                    Cursor cursor = GeckoApp.mAppContext.getContentResolver().query(
                        uri, 
                        new String[] { OpenableColumns.DISPLAY_NAME },
                        null, 
                        null, 
                        null);
                    String name = null;
                    if (cursor != null) {
                        try {
                            if (cursor.moveToNext()) {
                                name = cursor.getString(0);
                            }
                        } finally {
                            cursor.close();
                        }
                    }
                    String fileName = "tmp_";
                    String fileExt = null;
                    int period;
                    if (name == null || (period = name.lastIndexOf('.')) == -1) {
                        String mimeType = cr.getType(uri);
                        fileExt = "." + GeckoAppShell.getExtensionFromMimeType(mimeType);
                    } else {
                        fileExt = name.substring(period);
                        fileName = name.substring(0, period);
                    }
                    File file = File.createTempFile(fileName, fileExt, sGREDir);

                    FileOutputStream fos = new FileOutputStream(file);
                    InputStream is = cr.openInputStream(uri);
                    byte[] buf = new byte[4096];
                    int len = is.read(buf);
                    while (len != -1) {
                        fos.write(buf, 0, len);
                        len = is.read(buf);
                    }
                    fos.close();
                    filePickerResult =  file.getAbsolutePath();
                }catch (Exception e) {
                    Log.e(LOG_FILE_NAME, "showing file picker", e);
                }
            }
            try {
                mFilePickerResult.put(filePickerResult);
            } catch (InterruptedException e) {
                Log.i(LOG_FILE_NAME, "error returning file picker result", e);
            }
            break;
        case AWESOMEBAR_REQUEST:
            if (data != null) {
                String url = data.getStringExtra(AwesomeBar.URL_KEY);
                AwesomeBar.Type type = AwesomeBar.Type.valueOf(data.getStringExtra(AwesomeBar.TYPE_KEY));
                if (url != null && url.length() > 0) {
                    mBrowserToolbar.setProgressVisibility(true);
                    mBrowserToolbar.updateProgress(-1, -1);
                    loadUrl(url, type);
                }
            }
            break;
        case CAMERA_CAPTURE_REQUEST:
            Log.i(LOG_FILE_NAME, "Returning from CAMERA_CAPTURE_REQUEST: " + resultCode);
            File file = new File(Environment.getExternalStorageDirectory(), "cameraCapture-" + Integer.toString(kCaptureIndex) + ".jpg");
            kCaptureIndex++;
            GeckoEvent e = new GeckoEvent("cameraCaptureDone", resultCode == Activity.RESULT_OK ?
                                          "{\"ok\": true,  \"path\": \"" + file.getPath() + "\" }" :
                                          "{\"ok\": false, \"path\": \"" + file.getPath() + "\" }");
            GeckoAppShell.sendEventToGecko(e);
            break;
       }
    }

    public void doCameraCapture() {
        File file = new File(Environment.getExternalStorageDirectory(), "cameraCapture-" + Integer.toString(kCaptureIndex) + ".jpg");

        Intent intent = new Intent(android.provider.MediaStore.ACTION_IMAGE_CAPTURE);
        intent.putExtra(MediaStore.EXTRA_OUTPUT, Uri.fromFile(file));

        startActivityForResult(intent, CAMERA_CAPTURE_REQUEST);
    }

    public void loadUrl(String url, AwesomeBar.Type type) {
        mBrowserToolbar.setTitle(url);
        Log.d(LOG_FILE_NAME, type.name());
        if (type == AwesomeBar.Type.ADD) {
            GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Add", url));
        } else {
            GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Load", url));
        }
    }
    
    // Adapter to bind tabs into a list 
    private class TabsAdapter extends BaseAdapter {
	public TabsAdapter(Context context, HashMap<Integer, Tab> tabs) {
            mContext = context;
            mTabs = new ArrayList<Tab>();
            
            if (tabs != null) {
                Iterator keys = tabs.keySet().iterator();
                Tab tab;
                while (keys.hasNext()) {
                    tab = tabs.get(keys.next());
                    mTabs.add(tab);
                }
            }
           
            mInflater = LayoutInflater.from(mContext);
        }

        @Override    
        public int getCount() {
            return mTabs.size();
        }
    
        @Override    
        public Tab getItem(int position) {
            return mTabs.get(position);
        }

        @Override    
        public long getItemId(int position) {
            return position;
        }

        @Override    
        public View getView(int position, View convertView, ViewGroup parent) {

    	    if (convertView == null)
                convertView = mInflater.inflate(R.layout.tabs_row, null);

            Tab tab = mTabs.get(position);

            LinearLayout info = (LinearLayout) convertView.findViewById(R.id.info);
            info.setTag("" + tab.getId());
            info.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    hideTabs();
                    GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Select", "" + v.getTag()));
                }
            });

            ImageView favicon = (ImageView) convertView.findViewById(R.id.favicon);
            favicon.setImageDrawable(tab.getFavicon());

            TextView title = (TextView) convertView.findViewById(R.id.title);
            title.setText(tab.getTitle());

            TextView url = (TextView) convertView.findViewById(R.id.url);
            url.setText(tab.getURL());
            
            ImageButton close = (ImageButton) convertView.findViewById(R.id.close);
            if (mTabs.size() > 1) {
                close.setTag("" + tab.getId());
                close.setOnClickListener(new Button.OnClickListener() {
                    public void onClick(View v) {
                        int tabId = Integer.parseInt("" + v.getTag());
                        Tabs tabs = Tabs.getInstance();
                        Tab tab = tabs.getTab(tabId);

                        if (tabs.isSelectedTab(tab)) {
                            int index = tabs.getIndexOf(tab);
                            if (index >= 1)
                                index--;
                            else
                                index = 1;
                            int id = tabs.getTabAt(index).getId();
                            GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Select", "" + id));
                            GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Close", "" + v.getTag()));
                        } else {
                            GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Close", "" + v.getTag()));
                            GeckoAppShell.sendEventToGecko(new GeckoEvent("Tab:Select", "" + tabs.getSelectedTabId()));
                        }
                    }
                });
            } else {
                close.setVisibility(View.GONE);
            }

            return convertView;
        }

        @Override
        public void notifyDataSetChanged() {
        }

    
        private Context mContext;
        private ArrayList<Tab> mTabs;
        private LayoutInflater mInflater;
    }
}
