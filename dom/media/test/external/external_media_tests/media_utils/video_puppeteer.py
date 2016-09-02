# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from time import clock, sleep

from marionette import Marionette
from marionette_driver import By, expected, Wait

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
    These describe whatever stream is playing when the property is called.
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
        wait = Wait(self, timeout=self.timeout)
        verbose_until(wait, self, playback_started,
                      "Check if video has played some range")
        self._first_seen_time = self.current_time
        self._first_seen_wall_time = clock()
        self.update_expected_duration()

    def update_expected_duration(self):
        """
        Update the duration of the target video at self.test_url (in seconds).

        expected_duration represents the following: how long do we expect
        playback to last before we consider the video to be 'complete'?
        If we only want to play the first n seconds of the video,
        expected_duration is set to n.
        """
        # self.duration is the duration of whatever is playing right now.
        # In this case, we assume the video element always shows the same
        # stream throughout playback (i.e. the are no ads spliced into the main
        # video, for example), so self.duration is the duration of the main
        # video.
        video_duration = self.duration
        # Do our best to figure out where the video started playing
        played_ranges = self.played
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
        self.execute_video_script('arguments[0].wrappedJSObject.play();')

    def pause(self):
        """
        Tell the video element to Pause.
        """
        self.execute_video_script('arguments[0].wrappedJSObject.pause();')

    @property
    def duration(self):
        """
        :return: Duration in seconds of whatever stream is playing right now.
        """
        return self.execute_video_script('return arguments[0].'
                                         'wrappedJSObject.duration;') or 0

    @property
    def current_time(self):
        """
        :return: Current time of whatever stream is playing right now.
        """
        # Note that self.current_time could temporarily refer to a
        # spliced-in ad.

        return self.execute_video_script(
            'return arguments[0].wrappedJSObject.currentTime;') or 0

    @property
    def remaining_time(self):
        """
        :return: How much time is remaining given the duration of the video
            and the duration that has been set.
        """
        played_ranges = self.played
        # Playback should be in one range (as tests do not currently seek).
        assert played_ranges.length == 1
        played_duration = self.played.end(0) - self.played.start(0)
        return self.expected_duration - played_duration

    @property
    def played(self):
        """
        :return: A TimeRanges objected containing the played time ranges.
        """
        raw_time_ranges = self.execute_video_script(
            'var played = arguments[0].wrappedJSObject.played;'
            'var timeRanges = [];'
            'for (var i = 0; i < played.length; i++) {'
            'timeRanges.push([played.start(i), played.end(i)]);'
            '}'
            'return [played.length, timeRanges];')
        return TimeRanges(raw_time_ranges[0], raw_time_ranges[1])

    @property
    def video_src(self):
        """
        :return: The url of the actual video file, as opposed to the url
            of the page with the video element.
        """
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            return self.video.get_attribute('src')

    @property
    def total_frames(self):
        """
        :return: Number of video frames created and dropped since the creation
            of this video element.
        """
        return self.execute_video_script("""
            return arguments[0].getVideoPlaybackQuality()["totalVideoFrames"];
            """)

    @property
    def dropped_frames(self):
        """
        :return: Number of video frames created and dropped since the creation
            of this video element.
        """
        return self.execute_video_script("""return
            arguments[0].getVideoPlaybackQuality()["droppedVideoFrames"];
            """) or 0

    @property
    def corrupted_frames(self):
        """
        :return: Number of video frames corrupted since the creation of this
            video element. A corrupted frame may be created or dropped.
        """
        return self.execute_video_script("""return
            arguments[0].getVideoPlaybackQuality()["corruptedVideoFrames"];
            """) or 0

    @property
    def video_url(self):
        """
        :return: The URL of the video that this element is playing.
        """
        return self.execute_video_script('return arguments[0].baseURI;')

    @property
    def lag(self):
        """
        :return: The difference in time between where the video is currently
            playing and where it should be playing given the time we started
            the video.
        """
        # Note that self.current_time could temporarily refer to a
        # spliced-in ad
        elapsed_current_time = self.current_time - self._first_seen_time
        elapsed_wall_time = clock() - self._first_seen_wall_time
        return elapsed_wall_time - elapsed_current_time

    def measure_progress(self):
        initial = self.current_time
        sleep(1)
        return self.current_time - initial

    def execute_video_script(self, script):
        """
        Execute JS script in content context with access  to video element.

        :param script: script to be executed
        :return: value returned by script
        """
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            return self.marionette.execute_script(script,
                                                  script_args=[self.video])

    def __str__(self):
        messages = ['%s - test url: %s: {' % (type(self).__name__,
                                              self.test_url)]
        if self.video:
            messages += [
                '\t(video)',
                '\tcurrent_time: {},'.format(self.current_time),
                '\tduration: {},'.format(self.duration),
                '\texpected_duration: {},'.format(self.expected_duration),
                '\tplayed: {}'.format(self.played),
                '\tlag: {},'.format(self.lag),
                '\turl: {}'.format(self.video_url),
                '\tsrc: {}'.format(self.video_src),
                '\tframes total: {}'.format(self.total_frames),
                '\t - dropped: {}'.format(self.dropped_frames),
                '\t - corrupted: {}'.format(self.corrupted_frames)
            ]
        else:
            messages += ['\tvideo: None']
        messages.append('}')
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
        self.length = length
        self.ranges = [(pair[0], pair[1]) for pair in ranges]

    def __repr__(self):
        return 'TimeRanges: length: {}, ranges: {}'\
               .format(self.length, self.ranges)

    def start(self, index):
        return self.ranges[index][0]

    def end(self, index):
        return self.ranges[index][1]


def playback_started(video):
    """
    Determine if video has started

    :param video: The VideoPuppeteer instance that we are interested in.

    :return: True if is playing; False otherwise
    """
    try:
        played_ranges = video.played
        return played_ranges.length > 0 and \
               played_ranges.start(0) < played_ranges.end(0) and \
               played_ranges.end(0) > 0.0
    except Exception as e:
        print ('Got exception {}'.format(e))
        return False


def playback_done(video):
    """
    If we are near the end and there is still a video element, then
    we are essentially done. If this happens to be last time we are polled
    before the video ends, we won't get another chance.

    :param video: The VideoPuppeteer instance that we are interested in.

    :return: True if we are close enough to the end of playback; False
        otherwise.
    """
    if video.remaining_time < video.interval:
        return True

    # Check to see if the video has stalled. Accumulate the amount of lag
    # since the video started, and if it is too high, then raise.
    if video.stall_wait_time and (video.lag > video.stall_wait_time):
        raise VideoException('Video %s stalled.\n%s' % (video.video_url,
                                                        video))

    # We are cruising, so we are not done.
    return False
