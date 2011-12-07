/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
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

import android.app.Activity;
import android.os.Bundle;
import java.net.*;
import java.io.*;
import java.util.*;
import android.util.*;
import android.widget.*;
import android.net.*;
import android.content.Intent;

public class VideoPlayer extends Activity
{
    public static final String VIDEO_ACTION = "org.mozilla.gecko.PLAY_VIDEO";

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.videoplayer);
        mVideoView = (VideoView) findViewById(R.id.VideoView);
        MediaController mediaController = new MediaController(this);
        mediaController.setAnchorView(mVideoView);
        Intent intent = getIntent();
        Uri data = intent.getData();
        if (data == null)
            return;
        String spec = null;
        if ("vnd.youtube".equals(data.getScheme())) {
            String ssp = data.getSchemeSpecificPart();
            String id = ssp.substring(0, ssp.indexOf('?'));
            spec = getSpecFromYouTubeVideoID(id);
        }
        if (spec == null)
            return;
        Uri video = Uri.parse(spec);
        mVideoView.setMediaController(mediaController);
        mVideoView.setVideoURI(video);
        mVideoView.start();
    }

    VideoView mVideoView;

    String getSpecFromYouTubeVideoID(String id) {
        String spec = null;
        try {
            String info_uri = "http://www.youtube.com/get_video_info?&video_id=" + id;
            URL info_url = new URL(info_uri);
            URLConnection urlConnection = info_url.openConnection();
            BufferedReader br = new BufferedReader(new InputStreamReader(urlConnection.getInputStream()));
            try {
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = br.readLine()) != null)
                    sb.append(line);
                android.net.Uri fakeUri = android.net.Uri.parse("fake:/fake?" + sb);
                String stream_map = fakeUri.getQueryParameter("url_encoded_fmt_stream_map");
                if (stream_map == null)
                    return null;
                String[] streams = stream_map.split(",");
                for (int i = 0; i < streams.length; i++) {
                    fakeUri = android.net.Uri.parse("fake:/fake?" + streams[i]);
                    String url = fakeUri.getQueryParameter("url");
                    String type = fakeUri.getQueryParameter("type");
                    if (type != null && url != null &&
                        (type.startsWith("video/mp4") || type.startsWith("video/webm"))) {
                        spec = url;
                    }
                }
            } finally {
                br.close();
            }
        } catch (Exception e) {
            Log.e("VideoPlayer", "exception", e);
        }
        return spec;
    }
}
