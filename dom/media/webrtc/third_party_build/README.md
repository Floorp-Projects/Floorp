# Vendoring libwebrtc and the fast-forward process

Most of the important information about this process is contained on the fast-forward
automation wiki page
[here](https://wiki.mozilla.org/Media/WebRTC/libwebrtc_Update_Process/automation_plan).

To skip the history and details and go directly to starting the libwebrtc fast-foward
process, go to the
[Operation Checklist](https://wiki.mozilla.org/Media/WebRTC/libwebrtc_Update_Process/automation_plan#Operation_Checklist).

# Fixing errors reported in scripts

In most cases, the scripts report errors including suggestions on how to resolve the
issue.  If you're seeing an error message referring you to this README.md file, the
likely issue is that you're missing environment variables that should be set in a
config_env file in your root directory.  An example of that file can be found at
[dom/media/webrtc/third_party_build/example_config_env](https://searchfox.org/mozilla-central/rev/ef0aa879e94534ffd067a3748d034540a9fc10b0/dom/media/webrtc/third_party_build/example_config_env).
