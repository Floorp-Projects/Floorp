# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

    Partial reference: https://developers.google.com/youtube/iframe_api_reference.
    This reference is useful for site-specific features such as interacting
    with ads, or accessing YouTube's debug data.
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

    def __init__(self, marionette, url, **kwargs):
        self.player = None
        super(YouTubePuppeteer,
              self).__init__(marionette, url,
                             video_selector='.html5-video-player video',
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
            if (self.ad_inactive and self.duration and not
                    self.player_buffering):
                break
        self.update_expected_duration()

    def player_play(self):
        """
        Play via YouTube API.
        """
        self.execute_yt_script('arguments[1].wrappedJSObject.playVideo();')

    def player_pause(self):
        """
        Pause via YouTube API.
        """
        self.execute_yt_script('arguments[1].wrappedJSObject.pauseVideo();')

    @property
    def player_duration(self):
        """

        :return: Duration in seconds via YouTube API.
        """
        return self.execute_yt_script('return arguments[1].'
                                      'wrappedJSObject.getDuration();')

    @property
    def player_current_time(self):
        """

        :return: Current time in seconds via YouTube API.
        """
        return self.execute_yt_script('return arguments[1].'
                                      'wrappedJSObject.getCurrentTime();')

    @property
    def player_remaining_time(self):
        """

        :return: Remaining time in seconds via YouTube API.
        """
        return self.expected_duration - self.player_current_time

    def player_measure_progress(self):
        """

        :return: Playback progress in seconds via YouTube API.
        """
        initial = self.player_current_time
        sleep(1)
        return self.player_current_time - initial

    def _get_player_debug_dict(self):
        text = self.execute_yt_script('return arguments[1].'
                                      'wrappedJSObject.getDebugText();')
        if text:
            try:
                return loads(text)
            except ValueError:
                self.marionette.log('Error loading json: DebugText',
                                    level='DEBUG')

    def execute_yt_script(self, script):
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

    @property
    def playback_quality(self):
        """
        Please see https://developers.google.com/youtube/iframe_api_reference#Playback_quality
        for valid values.

        :return: A string with a valid value returned via YouTube.
        """
        return self.execute_yt_script('return arguments[1].'
                                      'wrappedJSObject.getPlaybackQuality();')

    @property
    def movie_id(self):
        """

        :return: The string that is the YouTube identifier.
        """
        return self.execute_yt_script('return arguments[1].'
                                      'wrappedJSObject.'
                                      'getVideoData()["video_id"];')

    @property
    def movie_title(self):
        """

        :return: The title of the movie.
        """
        title = self.execute_yt_script('return arguments[1].'
                                       'wrappedJSObject.'
                                       'getVideoData()["title"];')
        # title may include non-ascii characters; replace them to avoid
        # UnicodeEncodeError in string formatting for log messages
        return title.encode('ascii', 'replace')

    @property
    def player_url(self):
        """

        :return: The YouTube URL for the currently playing video.
        """
        return self.execute_yt_script('return arguments[1].'
                                      'wrappedJSObject.getVideoUrl();')

    @property
    def player_state(self):
        """

        :return: The YouTube state of the video. See
         https://developers.google.com/youtube/iframe_api_reference#getPlayerState
         for valid values.
        """
        state = self.execute_yt_script('return arguments[1].'
                                       'wrappedJSObject.getPlayerState();')
        return state

    @property
    def player_unstarted(self):
        """
        This and the following properties are based on the
        player.getPlayerState() call
        (https://developers.google.com/youtube/iframe_api_reference#Playback_status)

        :return: True if the video has not yet started.
        """
        return self.player_state == self._yt_player_state['UNSTARTED']

    @property
    def player_ended(self):
        """

        :return: True if the video playback has ended.
        """
        return self.player_state == self._yt_player_state['ENDED']

    @property
    def player_playing(self):
        """

        :return: True if the video is playing.
        """
        return self.player_state == self._yt_player_state['PLAYING']

    @property
    def player_paused(self):
        """

        :return: True if the video is paused.
        """
        return self.player_state == self._yt_player_state['PAUSED']

    @property
    def player_buffering(self):
        """

        :return: True if the video is currently buffering.
        """
        return self.player_state == self._yt_player_state['BUFFERING']

    @property
    def player_cued(self):
        """

        :return: True if the video is cued.
        """
        return self.player_state == self._yt_player_state['CUED']

    @property
    def ad_state(self):
        """
        Get state of current ad.

        :return: Returns one of the constants listed in
         https://developers.google.com/youtube/iframe_api_reference#Playback_status
         for an ad.

        """
        # Note: ad_state is sometimes not accurate, due to some sort of lag?
        return self.execute_yt_script('return arguments[1].'
                                      'wrappedJSObject.getAdState();')

    @property
    def ad_format(self):
        """
        When ad is not playing, ad_format is False.

        :return: integer representing ad format, or False
        """
        state = self.get_ad_displaystate()
        ad_format = False
        if state:
            with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
                ad_format = self.marionette.execute_script("""
                    return arguments[0].adFormat;""",
                    script_args=[state])
        return ad_format

    @property
    def ad_skippable(self):
        """

        :return: True if the current ad is skippable.
        """
        state = self.get_ad_displaystate()
        skippable = False
        if state:
            with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
                skippable = self.marionette.execute_script("""
                    return arguments[0].skippable;""",
                    script_args=[state])
        return skippable

    def get_ad_displaystate(self):
        # may return None
        return self.execute_yt_script('return arguments[1].'
                                      'wrappedJSObject.'
                                      'getOption("ad", "displaystate");')

    @property
    def breaks_count(self):
        """

        :return: Number of upcoming ad breaks.
        """
        breaks = self.execute_yt_script('return arguments[1].'
                                        'wrappedJSObject.'
                                        'getOption("ad", "breakscount")')
        # if video is not associated with any ads, breaks will be null
        return breaks or 0

    @property
    def ad_inactive(self):
        """

        :return: True if the current ad is inactive.
        """
        return (self.ad_ended or
                self.ad_state == self._yt_player_state['UNSTARTED'])

    @property
    def ad_playing(self):
        """

        :return: True if an ad is playing.
        """
        return self.ad_state == self._yt_player_state['PLAYING']

    @property
    def ad_ended(self):
        """

        :return: True if the current ad has ended.
        """
        return self.ad_state == self._yt_player_state['ENDED']

    def process_ad(self):
        """
        Try to skip this ad. If not, wait for this ad to finish.
        """
        if self.attempt_ad_skip() or self.ad_inactive:
            return
        ad_timeout = (self.search_ad_duration() or 30) + 5
        wait = Wait(self, timeout=ad_timeout, interval=1)
        try:
            self.marionette.log('process_ad: waiting %s s for ad' % ad_timeout)
            verbose_until(wait, self, lambda y: y.ad_ended, "Check if ad ended")
        except TimeoutException:
            self.marionette.log('Waiting for ad to end timed out',
                                level='WARNING')

    def attempt_ad_skip(self):
        """
        Attempt to skip ad by clicking on skip-add button.

        :return: True if clicking of ad-skip button occurred.
        """
        if self.ad_playing:
            self.marionette.log('Waiting while ad plays')
            sleep(10)
        else:
            # no ad playing
            return False
        if self.ad_skippable:
            selector = '.html5-video-player .videoAdUiSkipContainer'
            wait = Wait(self.marionette, timeout=30)
            try:
                with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
                    wait.until(expected.element_displayed(By.CSS_SELECTOR,
                                                          selector))
                    ad_button = self.marionette.find_element(By.CSS_SELECTOR,
                                                             selector)
                    ad_button.click()
                    self.marionette.log('Skipped ad.')
                    return True
            except (TimeoutException, NoSuchElementException):
                self.marionette.log('Could not obtain '
                                    'element: %s' % selector,
                                    level='WARNING')
        return False

    def search_ad_duration(self):
        """

        :return: ad duration in seconds, if currently displayed in player
        """
        if not (self.ad_playing or self.player_measure_progress() == 0):
            return None
        # If the ad is not Flash...
        if (self.ad_playing and self.video_src.startswith('mediasource') and
                self.duration):
            return self.duration
        selector = '.html5-media-player .videoAdUiAttribution'
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
                                'element: %s' % selector,
                                level='WARNING')
        return None

    @property
    def player_stalled(self):
        """

        :return: True if playback is not making progress for 4-9 seconds. This
         excludes ad breaks. Note that the player might just be busy with
         buffering due to a slow network.
        """

        # `current_time` stands still while ad is playing
        def condition():
            # no ad is playing and current_time stands still
            return (not self.ad_playing and
                    self.measure_progress() < 0.1 and
                    self.player_measure_progress() < 0.1 and
                    (self.player_playing or self.player_buffering))

        if condition():
            sleep(2)
            if self.player_buffering:
                sleep(5)
            return condition()
        else:
            return False

    def deactivate_autoplay(self):
        """
        Attempt to turn off autoplay.

        :return: True if successful.
        """
        element_id = 'autoplay-checkbox'
        mn = self.marionette
        wait = Wait(mn, timeout=10)

        def get_status(el):
            script = 'return arguments[0].wrappedJSObject.checked'
            return mn.execute_script(script, script_args=[el])

        try:
            with mn.using_context(Marionette.CONTEXT_CONTENT):
                # the width, height of the element are 0, so it's not visible
                wait.until(expected.element_present(By.ID, element_id))
                checkbox = mn.find_element(By.ID, element_id)

                # Note: in some videos, due to late-loading of sidebar ads, the
                # button is rerendered after sidebar ads appear & the autoplay
                # pref resets to "on". In other words, if you click too early,
                # the pref might get reset moments later.
                sleep(1)
                if get_status(checkbox):
                    mn.execute_script('return arguments[0].'
                                      'wrappedJSObject.click()',
                                      script_args=[checkbox])
                    self.marionette.log('Toggled autoplay.')
                autoplay = get_status(checkbox)
                self.marionette.log('Autoplay is %s' % autoplay)
                return (autoplay is not None) and (not autoplay)
        except (NoSuchElementException, TimeoutException):
            return False

    def __str__(self):
        messages = [super(YouTubePuppeteer, self).__str__()]
        if self.player:
            player_state = self._yt_player_state_name[self.player_state]
            ad_state = self._yt_player_state_name[self.ad_state]
            messages += [
                '.html5-media-player: {',
                '\tvideo id: {0},'.format(self.movie_id),
                '\tvideo_title: {0}'.format(self.movie_title),
                '\tcurrent_state: {0},'.format(player_state),
                '\tad_state: {0},'.format(ad_state),
                '\tplayback_quality: {0},'.format(self.playback_quality),
                '\tcurrent_time: {0},'.format(self.player_current_time),
                '\tduration: {0},'.format(self.player_duration),
                '}'
            ]
        else:
            messages += ['\t.html5-media-player: None']
        return '\n'.join(messages)


def playback_started(yt):
    """
    Check whether playback has started.

    :param yt: YouTubePuppeteer
    """
    # usually, ad is playing during initial buffering
    if (yt.player_state in
            [yt._yt_player_state['PLAYING'],
             yt._yt_player_state['BUFFERING']]):
        return True
    if yt.current_time > 0 or yt.player_current_time > 0:
        return True
    return False


def playback_done(yt):
    """
    Check whether playback is done, skipping ads if possible.

    :param yt: YouTubePuppeteer
    """
    # in case ad plays at end of video
    if yt.ad_state == yt._yt_player_state['PLAYING']:
        yt.attempt_ad_skip()
        return False
    return yt.player_ended or yt.player_remaining_time < 1


def wait_for_almost_done(yt, final_piece=120):
    """
    Allow the given video to play until only `final_piece` seconds remain,
    skipping ads mid-way as much as possible.
    `final_piece` should be short enough to not be interrupted by an ad.

    Depending on the length of the video, check the ad status every 10-30
    seconds, skip an active ad if possible.

    :param yt: YouTubePuppeteer
    """
    rest = 10
    duration = remaining_time = yt.expected_duration
    if duration < final_piece:
        # video is short so don't attempt to skip more ads
        return duration
    elif duration > 600:
        # for videos that are longer than 10 minutes
        # wait longer between checks
        rest = duration/50

    while remaining_time > final_piece:
        if yt.player_stalled:
            if yt.player_buffering:
                # fall back on timeout in 'wait' call that comes after this
                # in test function
                yt.marionette.log('Buffering and no playback progress.')
                break
            else:
                message = '\n'.join(['Playback stalled', str(yt)])
                raise VideoException(message)
        if yt.breaks_count > 0:
            yt.process_ad()
        if remaining_time > 1.5 * rest:
            sleep(rest)
        else:
            sleep(rest/2)
        # TODO during an ad, remaining_time will be based on ad's current_time
        # rather than current_time of target video
        remaining_time = yt.player_remaining_time
    return remaining_time
