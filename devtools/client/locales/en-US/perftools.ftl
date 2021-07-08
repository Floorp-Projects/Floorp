# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### These strings are used in DevTools’ performance-new panel, about:profiling, and
### the remote profiling panel. There are additional profiler strings in the appmenu.ftl
### file that are used for the profiler popup.

perftools-intro-title = Profiler Settings
perftools-intro-description =
  Recordings launch profiler.firefox.com in a new tab. All data is stored
  locally, but you can choose to upload it for sharing.

## All of the headings for the various sections.

perftools-heading-settings = Full Settings
perftools-heading-buffer = Buffer Settings
perftools-heading-features = Features
perftools-heading-features-default = Features (Recommended on by default)
perftools-heading-features-disabled = Disabled Features
perftools-heading-features-experimental = Experimental
perftools-heading-threads = Threads
perftools-heading-local-build = Local build

##

perftools-description-intro =
  Recordings launch <a>profiler.firefox.com</a> in a new tab. All data is stored
  locally, but you can choose to upload it for sharing.
perftools-description-local-build =
  If you’re profiling a build that you have compiled yourself, on this
  machine, please add your build’s objdir to the list below so that
  it can be used to look up symbol information.

## The controls for the interval at which the profiler samples the code.

perftools-range-interval-label = Sampling interval:
perftools-range-interval-milliseconds = {NUMBER($interval, maxFractionalUnits: 2)} ms

##

# The size of the memory buffer used to store things in the profiler.
perftools-range-entries-label = Buffer size:

perftools-custom-threads-label = Add custom threads by name:

perftools-devtools-interval-label = Interval:
perftools-devtools-threads-label = Threads:
perftools-devtools-settings-label = Settings

## Various statuses that affect the current state of profiling, not typically displayed.

perftools-status-private-browsing-notice =
  The profiler is disabled when Private Browsing is enabled.
  Close all Private Windows to re-enable the profiler
perftools-status-recording-stopped-by-another-tool = The recording was stopped by another tool.
perftools-status-restart-required = The browser must be restarted to enable this feature.

## These are shown briefly when the user is waiting for the profiler to respond.

perftools-request-to-stop-profiler = Stopping recording
perftools-request-to-get-profile-and-stop-profiler = Capturing profile

##

perftools-button-start-recording = Start recording
perftools-button-capture-recording = Capture recording
perftools-button-cancel-recording = Cancel recording
perftools-button-save-settings = Save settings and go back
perftools-button-restart = Restart
perftools-button-add-directory = Add a directory
perftools-button-remove-directory = Remove selected
perftools-button-edit-settings = Edit Settings…

## These messages are descriptions of the threads that can be enabled for the profiler.

perftools-thread-gecko-main =
  .title = The main processes for both the parent process, and content processes
perftools-thread-compositor =
  .title = Composites together different painted elements on the page
perftools-thread-dom-worker =
  .title = This handles both web workers and service workers
perftools-thread-renderer =
  .title = When WebRender is enabled, the thread that executes OpenGL calls
perftools-thread-render-backend =
  .title = The WebRender RenderBackend thread
perftools-thread-paint-worker =
  .title = When off-main-thread painting is enabled, the thread on which painting happens
perftools-thread-style-thread =
  .title = Style computation is split into multiple threads
pref-thread-stream-trans =
  .title = Network stream transport
perftools-thread-socket-thread =
  .title = The thread where networking code runs any blocking socket calls
perftools-thread-img-decoder =
  .title = Image decoding threads
perftools-thread-dns-resolver =
  .title = DNS resolution happens on this thread
perftools-thread-task-controller =
  .title = TaskController thread pool threads

##

perftools-record-all-registered-threads =
  Bypass selections above and record all registered threads

perftools-tools-threads-input-label =
  .title = These thread names are a comma separated list that is used to enable profiling of the threads in the profiler. The name needs to be only a partial match of the thread name to be included. It is whitespace sensitive.

## Onboarding UI labels. These labels are displayed in the new performance panel UI, when
## both devtools.performance.new-panel-onboarding & devtools.performance.new-panel-enabled
## preferences are true.

perftools-onboarding-message = <b>New</b>: { -profiler-brand-name } is now integrated into Developer Tools. <a>Learn more</a> about this powerful new tool.

# `options-context-advanced-settings` is defined in toolbox-options.ftl
perftools-onboarding-reenable-old-panel = (For a limited time, you can access the original Performance panel via <a>{ options-context-advanced-settings }</a>)

perftools-onboarding-close-button =
  .aria-label = Close the onboarding message
