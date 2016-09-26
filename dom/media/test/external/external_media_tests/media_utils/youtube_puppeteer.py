# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from collections import namedtuple
from time import sleep
import re
from json import loads

from marionette import Marionette
from marionette_driver import By, expected, Wait
from marionette_driver.errors import TimeoutException, NoSuchElementException
from video_puppeteer import VideoPuppeteer, VideoException
from external_media_tests.utils import verbose_until


class YouTubePuppeteer(VideoPuppeteer):
    """
    Wrapper around a YouTube .html5-video-player element.

    Can be used with youtube videos or youtube videos at embedded URLS. E.g.
    both https://www.youtube.com/watch?v=AbAACm1IQE0 and
    https://www.youtube.com/embed/AbAACm1IQE0 should work.

    Using an embedded video has the advantage of not auto-playing more videos
    while a test is running.

    Compared to video puppeteer, this class has the benefit of accessing the
    youtube player object as well as the video element. The YT player will
    report information for the underlying video even if an add is playing (as
    opposed to the video element behaviour, which will report on whatever
    is play at the time of query), and can also report if an ad is playing.

    Partial reference: https://developers.google.com/youtube/iframe_api_reference.
    This reference is useful for site-specific features such as interacting
    with ads, or accessing YouTube's debug data.
    """

    _player_var_script = (
        'var player_duration = arguments[1].wrappedJSObject.getDuration();'
        'var player_current_time = '
        'arguments[1].wrappedJSObject.getCurrentTime();'
        'var player_playback_quality = '
        'arguments[1].wrappedJSObject.getPlaybackQuality();'
        'var player_movie_id = '
        'arguments[1].wrappedJSObject.getVideoData()["video_id"];'
        'var player_movie_title = '
        'arguments[1].wrappedJSObject.getVideoData()["title"];'
        'var player_url = '
        'arguments[1].wrappedJSObject.getVideoUrl();'
        'var player_state = '
        'arguments[1].wrappedJSObject.getPlayerState();'
        'var player_ad_state = arguments[1].wrappedJSObject.getAdState();'
        'var player_breaks_count = '
        'arguments[1].wrappedJSObject.getOption("ad", "breakscount");'
    )
    """
    A string containing JS that will assign player state to
    variables. This is similar to `_video_var_script` from
    `VideoPuppeteer`. See `_video_var_script` for more information on the
    motivation for this method.

    This script assigns a subset of the vars used later by the
    `_yt_state_named_tuple` function. Please see that functions
    documentation for further information on these variables.
    """

    _yt_player_state = {
        'UNSTARTED': -1,
        'ENDED': 0,
        'PLAYING': 1,
        'PAUSED': 2,
        'BUFFERING': 3,
        'CUED': 5
    }
    _yt_player_state_name = {v: k for k, v in _yt_player_state.items()}
    _time_pattern = re.compile('(?P<minute>\d+):(?P<second>\d+)')

    def __init__(self, marionette, url, autostart=True, **kwargs):
        self.player = None
        self._last_seen_player_state = None
        super(YouTubePuppeteer,
              self).__init__(marionette, url,
                             video_selector='.html5-video-player video',
                             autostart=False,
                             **kwargs)
        wait = Wait(self.marionette, timeout=30)
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            verbose_until(wait, self,
                          expected.element_present(By.CLASS_NAME,
                                                   'html5-video-player'))
            self.player = self.marionette.find_element(By.CLASS_NAME,
                                                       'html5-video-player')
            self.marionette.execute_script("log('.html5-video-player "
                                           "element obtained');")
        # When an ad is playing, self.player_duration indicates the duration
        # of the spliced-in ad stream, not the duration of the main video, so
        # we attempt to skip the ad first.
        for attempt in range(5):
            sleep(1)
            self.process_ad()
            if (self._last_seen_player_state.player_ad_inactive and
                    self._last_seen_video_state.duration and not
                    self._last_seen_player_state.player_buffering):
                break
        self._update_expected_duration()
        if autostart:
            self.start()

    def player_play(self):
        """
        Play via YouTube API.
        """
        self._execute_yt_script('arguments[1].wrappedJSObject.playVideo();')

    def player_pause(self):
        """
        Pause via YouTube API.
        """
        self._execute_yt_script('arguments[1].wrappedJSObject.pauseVideo();')

    def _player_measure_progress(self):
        """
        Determine player progress. Refreshes state.

        :return: Playback progress in seconds via YouTube API with snapshots.
        """
        self._refresh_state()
        initial = self._last_seen_player_state.player_current_time
        sleep(1)
        self._refresh_state()
        return self._last_seen_player_state.player_current_time - initial

    def _get_player_debug_dict(self):
        text = self._execute_yt_script('return arguments[1].'
                                       'wrappedJSObject.getDebugText();')
        if text:
            try:
                return loads(text)
            except ValueError:
                self.marionette.log('Error loading json: DebugText',
                                    level='DEBUG')

    def _execute_yt_script(self, script):
        """
        Execute JS script in content context with access to video element and
        YouTube .html5-video-player element.

        :param script: script to be executed.

        :return: value returned by script
        """
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            return self.marionette.execute_script(script,
                                                  script_args=[self.video,
                                                               self.player])

    def _check_if_ad_ended(self):
        self._refresh_state()
        return self._last_seen_player_state.player_ad_ended

    def process_ad(self):
        """
        Wait for this ad to finish. Refreshes state.
        """
        self._refresh_state()
        if self._last_seen_player_state.player_ad_inactive:
            return
        ad_timeout = (self._search_ad_duration() or 30) + 5
        wait = Wait(self, timeout=ad_timeout, interval=1)
        try:
            self.marionette.log('process_ad: waiting {} s for ad'
                                .format(ad_timeout))
            verbose_until(wait,
                          self,
                          YouTubePuppeteer._check_if_ad_ended,
                          "Check if ad ended")
        except TimeoutException:
            self.marionette.log('Waiting for ad to end timed out',
                                level='WARNING')

    def _search_ad_duration(self):
        """
        Try and determine ad duration. Refreshes state.

        :return: ad duration in seconds, if currently displayed in player
        """
        self._refresh_state()
        if not (self._last_seen_player_state.player_ad_playing or
                self._player_measure_progress() == 0):
            return None
        if (self._last_seen_player_state.player_ad_playing and
                self._last_seen_video_state.duration):
            return self._last_seen_video_state.duration
        selector = '.html5-video-player .videoAdUiAttribution'
        wait = Wait(self.marionette, timeout=5)
        try:
            with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
                wait.until(expected.element_present(By.CSS_SELECTOR,
                                                    selector))
                countdown = self.marionette.find_element(By.CSS_SELECTOR,
                                                         selector)
                ad_time = self._time_pattern.search(countdown.text)
                if ad_time:
                    ad_minutes = int(ad_time.group('minute'))
                    ad_seconds = int(ad_time.group('second'))
                    return 60 * ad_minutes + ad_seconds
        except (TimeoutException, NoSuchElementException):
            self.marionette.log('Could not obtain '
                                'element: {}'.format(selector),
                                level='WARNING')
        return None

    def _player_stalled(self):
        """
        Checks if the player has stalled. Refreshes state.

        :return: True if playback is not making progress for 4-9 seconds. This
         excludes ad breaks. Note that the player might just be busy with
         buffering due to a slow network.
        """

        # `current_time` stands still while ad is playing
        def condition():
            # no ad is playing and current_time stands still
            return (not self._last_seen_player_state.player_ad_playing and
                    self._measure_progress() < 0.1 and
                    self._player_measure_progress() < 0.1 and
                    (self._last_seen_player_state.player_playing or
                     self._last_seen_player_state.player_buffering))

        if condition():
            sleep(2)
            self._refresh_state()
            if self._last_seen_player_state.player_buffering:
                sleep(5)
                self._refresh_state()
            return condition()
        else:
            return False

    @staticmethod
    def _yt_state_named_tuple():
        """
        Create a named tuple class that can be used to store state snapshots
        of the wrapped youtube player. The fields in the tuple should be used
        as follows:

        player_duration: the duration as fetched from the wrapped player.
        player_current_time: the current playback time as fetched from the
        wrapped player.
        player_remaining_time: the remaining time as calculated based on the
        puppeteers expected time and the players current time.
        player_playback_quality: the playback quality as fetched from the
        wrapped player. See:
        https://developers.google.com/youtube/js_api_reference#Playback_quality
        player_movie_id: the movie id fetched from the wrapped player.
        player_movie_title: the title fetched from the wrapped player.
        player_url: the self reported url fetched from the wrapped player.
        player_state: the current state of playback as fetch from the wrapped
        player. See:
        https://developers.google.com/youtube/js_api_reference#Playback_status
        player_unstarted, player_ended, player_playing, player_paused,
        player_buffering, and player_cued: these are all shortcuts to the
        player state, only one should be true at any time.
        player_ad_state: as player_state, but reports for the current ad.
        player_ad_state, player_ad_inactive, player_ad_playing, and
        player_ad_ended: these are all shortcuts to the ad state, only one
        should be true at any time.
        player_breaks_count: the number of ads as fetched from the wrapped
        player. This includes both played and unplayed ads, and includes
        streaming ads as well as pop up ads.

        :return: A 'player_state_info' named tuple class.
        """
        return namedtuple('player_state_info',
                          ['player_duration',
                           'player_current_time',
                           'player_remaining_time',
                           'player_playback_quality',
                           'player_movie_id',
                           'player_movie_title',
                           'player_url',
                           'player_state',
                           'player_unstarted',
                           'player_ended',
                           'player_playing',
                           'player_paused',
                           'player_buffering',
                           'player_cued',
                           'player_ad_state',
                           'player_ad_inactive',
                           'player_ad_playing',
                           'player_ad_ended',
                           'player_breaks_count'
                           ])

    def _create_player_state_info(self, **player_state_info_kwargs):
        """
        Create an instance of the state info named tuple. This function
        expects a dictionary containing the following keys:
        player_duration, player_current_time, player_playback_quality,
        player_movie_id, player_movie_title, player_url, player_state,
        player_ad_state, and player_breaks_count.

        For more information on the above keys and their values see
        `_yt_state_named_tuple`.

        :return: A named tuple 'yt_state_info', derived from arguments and
        state information from the puppeteer.
        """
        player_state_info_kwargs['player_remaining_time'] = (
            self.expected_duration -
            player_state_info_kwargs['player_current_time'])
        # Calculate player state convenience info
        player_state = player_state_info_kwargs['player_state']
        player_state_info_kwargs['player_unstarted'] = (
            player_state == self._yt_player_state['UNSTARTED'])
        player_state_info_kwargs['player_ended'] = (
            player_state == self._yt_player_state['ENDED'])
        player_state_info_kwargs['player_playing'] = (
            player_state == self._yt_player_state['PLAYING'])
        player_state_info_kwargs['player_paused'] = (
            player_state == self._yt_player_state['PAUSED'])
        player_state_info_kwargs['player_buffering'] = (
            player_state == self._yt_player_state['BUFFERING'])
        player_state_info_kwargs['player_cued'] = (
            player_state == self._yt_player_state['CUED'])
        # Calculate ad state convenience info
        player_ad_state = player_state_info_kwargs['player_ad_state']
        player_state_info_kwargs['player_ad_inactive'] = (
            player_ad_state == self._yt_player_state['UNSTARTED'])
        player_state_info_kwargs['player_ad_playing'] = (
            player_ad_state == self._yt_player_state['PLAYING'])
        player_state_info_kwargs['player_ad_ended'] = (
            player_ad_state == self._yt_player_state['ENDED'])
        # Create player snapshot
        state_info = self._yt_state_named_tuple()
        return state_info(**player_state_info_kwargs)

    @property
    def _fetch_state_script(self):
        if not self._fetch_state_script_string:
            self._fetch_state_script_string = (
                self._video_var_script +
                self._player_var_script +
                'return ['
                'currentTime,'
                'duration,'
                '[buffered.length, bufferedRanges],'
                '[played.length, playedRanges],'
                'totalFrames,'
                'droppedFrames,'
                'corruptedFrames,'
                'player_duration,'
                'player_current_time,'
                'player_playback_quality,'
                'player_movie_id,'
                'player_movie_title,'
                'player_url,'
                'player_state,'
                'player_ad_state,'
                'player_breaks_count];')
        return self._fetch_state_script_string

    def _refresh_state(self):
        """
        Refresh the snapshot of the underlying video and player state. We do
        this allin one so that the state doesn't change in between queries.

        We also store information that can be derived from the snapshotted
        information, such as lag. This is stored in the last seen state to
        stress that it's based on the snapshot.
        """
        values = self._execute_yt_script(self._fetch_state_script)
        video_keys = ['current_time', 'duration', 'raw_buffered_ranges',
                      'raw_played_ranges', 'total_frames', 'dropped_frames',
                      'corrupted_frames']
        player_keys = ['player_duration', 'player_current_time',
                       'player_playback_quality', 'player_movie_id',
                       'player_movie_title', 'player_url', 'player_state',
                       'player_ad_state', 'player_breaks_count']
        # Get video state
        self._last_seen_video_state = (
            self._create_video_state_info(**dict(
                zip(video_keys, values[:len(video_keys)]))))
        # Get player state
        self._last_seen_player_state = (
            self._create_player_state_info(**dict(
                zip(player_keys, values[-len(player_keys):]))))

    def mse_enabled(self):
        """
        Check if the video source indicates mse usage for current video.
        Refreshes state.

        :return: True if MSE is being used, False if not.
        """
        self._refresh_state()
        return self._last_seen_video_state.video_src.startswith('blob')

    def playback_started(self):
        """
        Check whether playback has started. Refreshes state.

        :return: True if play back has started, False if not.
        """
        self._refresh_state()
        # usually, ad is playing during initial buffering
        if (self._last_seen_player_state.player_playing or
                self._last_seen_player_state.player_buffering):
            return True
        if (self._last_seen_video_state.current_time > 0 or
                self._last_seen_player_state.player_current_time > 0):
            return True
        return False

    def playback_done(self):
        """
        Check whether playback is done. Refreshes state.

        :return: True if play back has ended, False if not.
        """
        # in case ad plays at end of video
        self._refresh_state()
        if self._last_seen_player_state.player_ad_playing:
            return False
        return (self._last_seen_player_state.player_ended or
                self._last_seen_player_state.player_remaining_time < 1)

    def wait_for_almost_done(self, final_piece=120):
        """
        Allow the given video to play until only `final_piece` seconds remain,
        skipping ads mid-way as much as possible.
        `final_piece` should be short enough to not be interrupted by an ad.

        Depending on the length of the video, check the ad status every 10-30
        seconds, skip an active ad if possible.

        This call refreshes state.

        :param final_piece: The length in seconds of the desired remaining time
        to wait until.
        """
        self._refresh_state()
        rest = 10
        duration = remaining_time = self.expected_duration
        if duration < final_piece:
            # video is short so don't attempt to skip more ads
            return duration
        elif duration > 600:
            # for videos that are longer than 10 minutes
            # wait longer between checks
            rest = duration / 50

        while remaining_time > final_piece:
            if self._player_stalled():
                if self._last_seen_player_state.player_buffering:
                    # fall back on timeout in 'wait' call that comes after this
                    # in test function
                    self.marionette.log('Buffering and no playback progress.')
                    break
                else:
                    message = '\n'.join(['Playback stalled', str(self)])
                    raise VideoException(message)
            if self._last_seen_player_state.player_breaks_count > 0:
                self.process_ad()
            if remaining_time > 1.5 * rest:
                sleep(rest)
            else:
                sleep(rest / 2)
            # TODO during an ad, remaining_time will be based on ad's current_time
            # rather than current_time of target video
            remaining_time = self._last_seen_player_state.player_remaining_time
        return remaining_time

    def __str__(self):
        messages = [super(YouTubePuppeteer, self).__str__()]
        if not self.player:
            messages += ['\t.html5-media-player: None']
            return '\n'.join(messages)
        if not self._last_seen_player_state:
            messages += ['\t.html5-media-player: No last seen state']
            return '\n'.join(messages)
        messages += ['.html5-media-player: {']
        for field in self._last_seen_player_state._fields:
            messages += [('\t{}: {}'
                          .format(field, getattr(self._last_seen_player_state,
                                                 field)))]
        messages += '}'
        return '\n'.join(messages)
