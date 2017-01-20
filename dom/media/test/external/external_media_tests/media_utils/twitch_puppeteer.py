# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from collections import namedtuple
from time import clock

from marionette_driver import By, expected, Wait
from marionette_harness import Marionette

from video_puppeteer import VideoPuppeteer, TimeRanges
from external_media_tests.utils import verbose_until


class TwitchPuppeteer(VideoPuppeteer):
    """
    Wrapper around a Twitch .player element.

    Note that the twitch stream needs to be playing for the puppeteer to work
    correctly. Since twitch will load a player element for streams that are
    not currently playing, the puppeteer will still detect a player element,
    however the time ranges will not increase, and tests will fail.

    Compared to video puppeteer, this class has the benefit of accessing the
    twitch player element as well as the video element. The twitch player
    element reports additional information to aid in testing -- such as if an
    ad is playing. However, the attributes of this element do not appear to be
    documented, which may leave them vulnerable to undocumented changes in
    behaviour.
    """
    _player_var_script = (
        'var player_data_screen = '
        'arguments[1].wrappedJSObject.getAttribute("data-screen");'
    )
    """
    A string containing JS that will assign player state to
    variables. This is similar to `_video_var_script` from
    `VideoPuppeteer`. See `_video_var_script` for more information on the
    motivation for this method.
    """

    def __init__(self, marionette, url, autostart=True,
                 set_duration=10.0, **kwargs):
        self.player = None
        self._last_seen_player_state = None
        super(TwitchPuppeteer,
              self).__init__(marionette, url, set_duration=set_duration,
                             autostart=False, **kwargs)
        wait = Wait(self.marionette, timeout=30)
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            verbose_until(wait, self,
                          expected.element_present(By.CLASS_NAME,
                                                   'player'))
            self.player = self.marionette.find_element(By.CLASS_NAME,
                                                       'player')
            self.marionette.execute_script("log('.player "
                                           "element obtained');")
            if autostart:
                self.start()

    def _update_expected_duration(self):
        if 0 < self._set_duration:
            self.expected_duration = self._set_duration
        else:
            # If we don't have a set duration we don't know how long is left
            # in the stream.
            self.expected_duration = float("inf")

    def _calculate_remaining_time(self, played_ranges):
        """
        Override of video_puppeteer's _calculate_remaining_time. See that
        method for primary documentation.

        This override is in place to adjust how remaining time is handled.
        Twitch ads can cause small stutters which result in multiple played
        ranges, despite no seeks being initiated by the tests. As such, when
        calculating the remaining time, the start time is the min of all
        played start times, and the end time is the max of played end times.
        This being sensible behaviour relies on the tests not attempting seeks.

        :param played_ranges: A TimeRanges object containing played ranges.
        :return: The remaining time expected for this puppeteer.
        """
        min_played_time = min(
            [played_ranges.start(i) for i in range(0, played_ranges.length)])
        max_played_time = max(
            [played_ranges.end(i) for i in range(0, played_ranges.length)])
        played_duration = max_played_time - min_played_time
        return self.expected_duration - played_duration

    def _execute_twitch_script(self, script):
        """
        Execute JS script in content context with access to video element and
        Twitch .player element.

        :param script: script to be executed.

        :return: value returned by script
        """
        with self.marionette.using_context(Marionette.CONTEXT_CONTENT):
            return self.marionette.execute_script(script,
                                                  script_args=[self.video,
                                                               self.player])

    @staticmethod
    def _twitch_state_named_tuple():
        """
        Create a named tuple class that can be used to store state snapshots
        of the wrapped twitch player. The fields in the tuple should be used
        as follows:

        player_data_screen: the current displayed content, appears to be set
        to nothing if no ad has been played, 'advertisement' during ad
        playback, and 'content' following ad playback.
        """
        return namedtuple('player_state_info',
                          ['player_data_screen'])

    def _create_player_state_info(self, **player_state_info_kwargs):
        """
        Create an instance of the state info named tuple. This function
        expects a dictionary containing the following keys:
        player_data_screen.

        For more information on the above keys and their values see
        `_twitch_state_named_tuple`.

        :return: A named tuple 'player_state_info', derived from arguments and
        state information from the puppeteer.
        """
        # Create player snapshot
        state_info = self._twitch_state_named_tuple()
        return state_info(**player_state_info_kwargs)

    @property
    def _fetch_state_script(self):
        if not self._fetch_state_script_string:
            self._fetch_state_script_string = (
                self._video_var_script +
                self._player_var_script +
                'return ['
                'baseURI,'
                'currentTime,'
                'duration,'
                '[buffered.length, bufferedRanges],'
                '[played.length, playedRanges],'
                'totalFrames,'
                'droppedFrames,'
                'corruptedFrames,'
                'player_data_screen];')
        return self._fetch_state_script_string

    def _refresh_state(self):
        """
        Refresh the snapshot of the underlying video and player state. We do
        this all in one so that the state doesn't change in between queries.

        We also store information thouat can be derived from the snapshotted
        information, such as lag. This is stored in the last seen state to
        stress that it's based on the snapshot.
        """
        values = self._execute_twitch_script(self._fetch_state_script)
        video_keys = ['base_uri', 'current_time', 'duration',
                      'raw_buffered_ranges', 'raw_played_ranges',
                      'total_frames', 'dropped_frames', 'corrupted_frames']
        player_keys = ['player_data_screen']
        # Get video state
        self._last_seen_video_state = (
            self._create_video_state_info(**dict(
                zip(video_keys, values[:len(video_keys)]))))
        # Get player state
        self._last_seen_player_state = (
            self._create_player_state_info(**dict(
                zip(player_keys, values[-len(player_keys):]))))

    def __str__(self):
        messages = [super(TwitchPuppeteer, self).__str__()]
        if not self.player:
            messages += ['\t.player: None']
            return '\n'.join(messages)
        if not self._last_seen_player_state:
            messages += ['\t.player: No last seen state']
            return '\n'.join(messages)
        messages += ['.player: {']
        for field in self._last_seen_player_state._fields:
            # For compatibility with different test environments we force
            # ascii
            field_ascii = (
                unicode(getattr(self._last_seen_player_state, field)).encode(
                    'ascii', 'replace'))
            messages += [('\t{}: {}'.format(field, field_ascii))]
        messages += '}'
        return '\n'.join(messages)
