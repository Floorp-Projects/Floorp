package org.mozilla.gecko;
import android.app.*;
import android.os.*;
import android.provider.*;
import android.content.*;
import android.widget.*;
import android.provider.*;
import android.database.*;
import android.view.*;
import android.util.*;

public class GeckoBookmarks extends ListActivity {

  static String[] sFromColumns = {Browser.BookmarkColumns.URL};
  static int[] sToColumns = {0};
  Cursor mCursor = null;

  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.bookmarks);
    mCursor = managedQuery(android.provider.Browser.BOOKMARKS_URI,
        		null, null, null, null); 
    startManagingCursor(mCursor);
    
    ListAdapter adapter = 
      new SimpleCursorAdapter(this, R.layout.bookmark_list_row, mCursor, 
			      new String[] {Browser.BookmarkColumns.TITLE, 
					    Browser.BookmarkColumns.URL},
			      new int[] {R.id.bookmark_title, R.id.bookmark_url}); 
    setListAdapter(adapter);
  }
 
  protected void onResume() {
    super.onResume();
  }

  protected void onPause() {
    super.onPause();
  }

  protected void onListItemClick(ListView l, View v, int position, long id) {
    mCursor.moveToPosition(position);
    String spec = mCursor.getString(mCursor.getColumnIndex(Browser.BookmarkColumns.URL));
    Log.i("GeckoBookmark", "clicked: " + spec);
    GeckoApp app = org.mozilla.gecko.GeckoApp.mAppContext;
    Intent intent = new Intent(this, app.getClass());
    intent.setAction(Intent.ACTION_VIEW);
    intent.setData(android.net.Uri.parse(spec));
    startActivity(intent);
  }

  public void addBookmark(View v) {
    Browser.saveBookmark(this, mTitle, mUri.toString());
    finish();
  }

  @Override
  protected void onNewIntent(Intent intent) {
    // just save the uri from the intent
    mUri = intent.getData();
    mTitle = intent.getStringExtra("title");
  }

  android.net.Uri mUri;
  String mTitle;
}