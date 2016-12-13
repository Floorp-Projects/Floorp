# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from collections import namedtuple
from time import clock, sleep

from marionette_driver import By, expected, Wait
from marionette_harness import Marionette

from external_media_tests.utils import verbose_until


# Adapted from
# https://github.com/gavinsharp/aboutmedia/blob/master/chrome/content/aboutmedia.xhtml
debug_script = """
var mainWindow = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
    .getInterface(Components.interfaces.nsIWebNavigation)
    .QueryInterface(Components.interfaces.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
    .getInterface(Components.interfaces.nsIDOMWindow);
var tabbrowser = mainWindow.gBrowser;
for (var i=0; i < tabbrowser.browsers.length; ++i) {
  var b = tabbrowser.getBrowserAtIndex(i);
  var media = b.contentDocumentAsCPOW.getElementsByTagName('video');
  for (var j=0; j < media.length; ++j) {
     var ms = media[j].mozMediaSourceObject;
     if (ms) {
       debugLines = ms.mozDebugReaderData.split(\"\\n\");
       return debugLines;
     }
  }
}"""


class VideoPuppeteer(object):
    """
    Wrapper to control and introspect HTML5 video elements.

    A note about properties like current_time and duration:
    These describe whatever stream is playing when the state is checked.
    It is possible that many different streams are dynamically spliced
    together, so the video stream that is currently playing might be the main
    video or it might be something else, like an ad, for example.

    :param marionette: The marionette instance this runs in.
    :param url: the URL of the page containing the video element.
    :param video_selector: the selector of the element that we want to watch.
     This is set by default to 'video', which is what most sites use, but
     others should work.
    :param interval: The polling interval that is used to check progress.
    :param set_duration: When set to >0, the polling and checking will stop at
     the number of seconds specified. Otherwise, this will stop at the end
     of the video.
    :param stall_wait_time: The amount of time to wait to see if a stall has
     cleared. If 0, do not check for stalls.
    :param timeout: The amount of time to wait until the video starts.
    """

    _video_var_script = (
        'var video = arguments[0];'
        'var baseURI = arguments[0].baseURI;'
        'var currentTime = video.wrappedJSObject.currentTime;'
        'var duration = video.wrappedJSObject.duration;'
        'var buffered = video.wrappedJSObject.buffered;'
        'var bufferedRanges = [];'
        'for (var i = 0; i < buffered.length; i++) {'
        'bufferedRanges.push([buffered.start(i), buffered.end(i)]);'
        '}'
        'var played = video.wrappedJSObject.played;'
        'var playedRanges = [];'
        'for (var i = 0; i < played.length; i++) {'
        'playedRanges.push([played.start(i), played.end(i)]);'
        '}'
        'var totalFrames = '
        'video.getVideoPlaybackQuality()["totalVideoFrames"];'
        'var droppedFrames = '
        'video.getVideoPlaybackQuality()["droppedVideoFrames"];'
        'var corruptedFrames = '
        'video.getVideoPlaybackQuality()["corruptedVideoFrames"];'
    )
    """
    A string containing JS that assigns video state to variables.
    The purpose of this string script is to be appended to by this and
    any inheriting classes to return these and other variables. In the case
    of an inheriting class the script can be added to in order to fetch
    further relevant variables -- keep in mind we only want one script
    execution to prevent races, so it wouldn't do to have child classes
    run this script then their own, as there is potential for lag in
    between.

    This script assigns a subset of the vars used later by the
    `_video_state_named_tuple` function. Please see that function's
    documentation for further information on these variables.
    """

    def __init__(self, marionette, url, video_selector='video', interval=1,
                 set_duration=0, stall_wait_time=0, timeout=60,
                 autostart=True):
        self.marionette = marionette
        self.test_url = url
        self.interval = interval
        self.stall_wait_time = stall_wait_time
        self.timeout = timeout
        self._set_duration = set_duration
        self.video = None
        self.expected_duration = 0
        self._first_seen_time = 0
        self._first_seen_wall_time = 0
        self._fetch_state_script_string = None
        self._last_seen_video_state = None
        wait = Wait(self.marionette, timeout=self.timeout)
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            self.marionette.navigate(self.test_url)
            self.marionette.execute_script("""
                log('URL: {0}');""".format(self.test_url))
            verbose_until(wait, self,
                          expected.element_present(By.TAG_NAME, 'video'))
            videos_found = self.marionette.find_elements(By.CSS_SELECTOR,
                                                         video_selector)
            if len(videos_found) > 1:
                self.marionette.log(type(self).__name__ + ': multiple video '
                                                          'elements found. '
                                                          'Using first.')
            if len(videos_found) <= 0:
                self.marionette.log(type(self).__name__ + ': no video '
                                                          'elements found.')
                return
            self.video = videos_found[0]
            self.marionette.execute_script("log('video element obtained');")
            if autostart:
                self.start()

    def start(self):
        # To get an accurate expected_duration, playback must have started
        self._refresh_state()
        wait = Wait(self, timeout=self.timeout)
        verbose_until(wait, self, VideoPuppeteer.playback_started,
                      "Check if video has played some range")
        self._first_seen_time = self._last_seen_video_state.current_time
        self._first_seen_wall_time = clock()
        self._update_expected_duration()

    def get_debug_lines(self):
        """
        Get Firefox internal debugging for the video element.

        :return: A text string that has Firefox-internal debugging information.
        """
        with self.marionette.using_context('chrome'):
            debug_lines = self.marionette.execute_script(debug_script)
        return debug_lines

    def play(self):
        """
        Tell the video element to Play.
        """
        self._execute_video_script('arguments[0].wrappedJSObject.play();')

    def pause(self):
        """
        Tell the video element to Pause.
        """
        self._execute_video_script('arguments[0].wrappedJSObject.pause();')

    def playback_started(self):
        """
        Determine if video has started

        :param self: The VideoPuppeteer instance that we are interested in.

        :return: True if is playing; False otherwise
        """
        self._refresh_state()
        try:
            played_ranges = self._last_seen_video_state.played
            return (
                played_ranges.length > 0 and
                played_ranges.start(0) < played_ranges.end(0) and
                played_ranges.end(0) > 0.0
            )
        except Exception as e:
            print ('Got exception {}'.format(e))
            return False

    def playback_done(self):
        """
        If we are near the end and there is still a video element, then
        we are essentially done. If this happens to be last time we are polled
        before the video ends, we won't get another chance.

        :param self: The VideoPuppeteer instance that we are interested in.

        :return: True if we are close enough to the end of playback; False
            otherwise.
        """
        self._refresh_state()

        if self._last_seen_video_state.remaining_time < self.interval:
            return True

        # Check to see if the video has stalled. Accumulate the amount of lag
        # since the video started, and if it is too high, then raise.
        if (self.stall_wait_time and
                self._last_seen_video_state.lag > self.stall_wait_time):
            raise VideoException('Video {} stalled.\n{}'
                                 .format(self._last_seen_video_state.video_uri,
                                         self))

        # We are cruising, so we are not done.
        return False

    def _update_expected_duration(self):
        """
        Update the duration of the target video at self.test_url (in seconds).
        This is based on the last seen state, so the state should be,
        refreshed at least once before this is called.

        expected_duration represents the following: how long do we expect
        playback to last before we consider the video to be 'complete'?
        If we only want to play the first n seconds of the video,
        expected_duration is set to n.
        """

        # self._last_seen_video_state.duration is the duration of whatever was
        # playing when the state was checked. In this case, we assume the video
        # element always shows the same stream throughout playback (i.e. the
        # are no ads spliced into the main video, for example), so
        # self._last_seen_video_state.duration is the duration of the main
        # video.
        video_duration = self._last_seen_video_state.duration
        # Do our best to figure out where the video started playing
        played_ranges = self._last_seen_video_state.played
        if played_ranges.length > 0:
            # If we have a range we should only have on continuous range
            assert played_ranges.length == 1
            start_position = played_ranges.start(0)
        else:
            # If we don't have a range we should have a current time
            start_position = self._first_seen_time
        # In case video starts at t > 0, adjust target time partial playback
        remaining_video = video_duration - start_position
        if 0 < self._set_duration < remaining_video:
            self.expected_duration = self._set_duration
        else:
            self.expected_duration = remaining_video

    @staticmethod
    def _video_state_named_tuple():
        """
        Create a named tuple class that can be used to store state snapshots
        of the wrapped element. The fields in the tuple should be used as
        follows:

        base_uri: the baseURI attribute of the wrapped element.
        current_time: the current time of the wrapped element.
        duration: the duration of the wrapped element.
        buffered: the buffered ranges of the wrapped element. In its raw form
        this is as a list where the first element is the length and the second
        element is a list of 2 item lists, where each two items are a buffered
        range. Once assigned to the tuple this data should be wrapped in the
        TimeRanges class.
        played: the played ranges of the wrapped element. In its raw form this
        is as a list where the first element is the length and the second
        element is a list of 2 item lists, where each two items are a played
        range. Once assigned to the tuple this data should be wrapped in the
        TimeRanges class.
        lag: the difference in real world time and wrapped element time.
        Calculated as real world time passed - element time passed.
        totalFrames: number of total frames for the wrapped element
        droppedFrames: number of dropped frames for the wrapped element.
        corruptedFrames: number of corrupted frames for the wrapped.
        video_src: the src attribute of the wrapped element.

        :return: A 'video_state_info' named tuple class.
        """
        return namedtuple('video_state_info',
                          ['base_uri',
                           'current_time',
                           'duration',
                           'remaining_time',
                           'buffered',
                           'played',
                           'lag',
                           'total_frames',
                           'dropped_frames',
                           'corrupted_frames',
                           'video_src'])

    def _create_video_state_info(self, **video_state_info_kwargs):
        """
        Create an instance of the video_state_info named tuple. This function
        expects a dictionary populated with the following keys: current_time,
        duration, raw_played_ranges, total_frames, dropped_frames, and
        corrupted_frames.

        Aside from raw_played_ranges, see `_video_state_named_tuple` for more
        information on the above keys and values. For raw_played_ranges a
        list is expected that can be consumed to make a TimeRanges object.

        :return: A named tuple 'video_state_info' derived from arguments and
        state information from the puppeteer.
        """
        raw_buffered_ranges = video_state_info_kwargs['raw_buffered_ranges']
        raw_played_ranges = video_state_info_kwargs['raw_played_ranges']
        # Remove raw ranges from dict as they are not used in the final named
        # tuple and will provide an unexpected kwarg if kept.
        del video_state_info_kwargs['raw_buffered_ranges']
        del video_state_info_kwargs['raw_played_ranges']
        # Create buffered ranges
        video_state_info_kwargs['buffered'] = (
            TimeRanges(raw_buffered_ranges[0], raw_buffered_ranges[1]))
        # Create played ranges
        video_state_info_kwargs['played'] = (
            TimeRanges(raw_played_ranges[0], raw_played_ranges[1]))
        # Calculate elapsed times
        elapsed_current_time = (video_state_info_kwargs['current_time'] -
                                self._first_seen_time)
        elapsed_wall_time = clock() - self._first_seen_wall_time
        # Calculate lag
        video_state_info_kwargs['lag'] = (
            elapsed_wall_time - elapsed_current_time)
        # Calculate remaining time
        if video_state_info_kwargs['played'].length > 0:
            played_duration = (video_state_info_kwargs['played'].end(0) -
                               video_state_info_kwargs['played'].start(0))
            video_state_info_kwargs['remaining_time'] = (
                self.expected_duration - played_duration)
        else:
            # No playback has happened yet, remaining time is duration
            video_state_info_kwargs['remaining_time'] = self.expected_duration
        # Fetch non time critical source information
        video_state_info_kwargs['video_src'] = self.video.get_attribute('src')
        # Create video state snapshot
        state_info = self._video_state_named_tuple()
        return state_info(**video_state_info_kwargs)

    @property
    def _fetch_state_script(self):
        if not self._fetch_state_script_string:
            self._fetch_state_script_string = (
                self._video_var_script +
                'return ['
                'baseURI,'
                'currentTime,'
                'duration,'
                '[buffered.length, bufferedRanges],'
                '[played.length, playedRanges],'
                'totalFrames,'
                'droppedFrames,'
                'corruptedFrames];')
        return self._fetch_state_script_string

    def _refresh_state(self):
        """
        Refresh the snapshot of the underlying video state. We do this all
        in one so that the state doesn't change in between queries.

        We also store information that can be derived from the snapshotted
        information, such as lag. This is stored in the last seen state to
        stress that it's based on the snapshot.
        """
        keys = ['base_uri', 'current_time', 'duration', 'raw_buffered_ranges',
                'raw_played_ranges', 'total_frames', 'dropped_frames',
                'corrupted_frames']
        values = self._execute_video_script(self._fetch_state_script)
        self._last_seen_video_state = (
            self._create_video_state_info(**dict(zip(keys, values))))

    def _measure_progress(self):
        self._refresh_state()
        initial = self._last_seen_video_state.current_time
        sleep(1)
        self._refresh_state()
        return self._last_seen_video_state.current_time - initial

    def _execute_video_script(self, script):
        """
        Execute JS script in content context with access  to video element.

        :param script: script to be executed
        :return: value returned by script
        """
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            return self.marionette.execute_script(script,
                                                  script_args=[self.video])

    def __str__(self):
        messages = ['{} - test url: {}: '
                    .format(type(self).__name__, self.test_url)]
        if not self.video:
            messages += ['\tvideo: None']
            return '\n'.join(messages)
        if not self._last_seen_video_state:
            messages += ['\tvideo: No last seen state']
            return '\n'.join(messages)
        # Have video and state info
        messages += [
            '{',
            '\t(video)'
        ]
        messages += ['\tinterval: {}'.format(self.interval)]
        messages += ['\texpected duration: {}'.format(self.expected_duration)]
        messages += ['\tstall wait time: {}'.format(self.stall_wait_time)]
        messages += ['\ttimeout: {}'.format(self.timeout)]
        # Print each field on its own line
        for field in self._last_seen_video_state._fields:
            # For compatibility with different test environments we force ascii
            field_ascii = (
                unicode(getattr(self._last_seen_video_state, field))
                .encode('ascii','replace'))
            messages += [('\t{}: {}'.format(field, field_ascii))]
        messages += '}'
        return '\n'.join(messages)


class VideoException(Exception):
    """
    Exception class to use for video-specific error processing.
    """
    pass


class TimeRanges:
    """
    Class to represent the TimeRanges data returned by played(). Exposes a
    similar interface to the JavaScript TimeRanges object.
    """
    def __init__(self, length, ranges):
        # These should be the same,. Theoretically we don't need the length,
        # but since this should be used to consume data coming back from
        # JS exec, this is a valid sanity check.
        assert length == len(ranges)
        self.length = length
        self.ranges = [(pair[0], pair[1]) for pair in ranges]

    def __repr__(self):
        return (
            'TimeRanges: length: {}, ranges: {}'
            .format(self.length, self.ranges)
        )

    def start(self, index):
        return self.ranges[index][0]

    def end(self, index):
        return self.ranges[index][1]
