/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
