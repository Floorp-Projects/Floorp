#!/usr/bin/env python3

import os

rates = [44100, 48000]
channels = [1, 2]
duration = "0.5"
frequency = "1000"
volume = "-3dB"
name = "half-a-second"
formats = {
    "aac-in-adts": [{"codec": "aac", "extension": "aac"}],
    "mp3": [{"codec": "libmp3lame", "extension": "mp3"}],
    "mp4": [
        {
            "codec": "libopus",
            "extension": "mp4",
        },
        {"codec": "aac", "extension": "mp4"},
    ],
    "ogg": [
        {"codec": "libvorbis", "extension": "ogg"},
        {"codec": "libopus", "extension": "opus"},
    ],
    "flac": [
        {"codec": "flac", "extension": "flac"},
    ],
    "webm": [
        {"codec": "libopus", "extension": "webm"},
        {"codec": "libvorbis", "extension": "webm"},
    ],
}

for rate in rates:
    for channel_count in channels:
        wav_filename = "{}-{}ch-{}.wav".format(name, channel_count, rate)
        wav_command = "sox -V -r {} -n -b 16 -c {} {}  synth {} sin {} vol {}".format(
            rate, channel_count, wav_filename, duration, frequency, volume
        )
        print(wav_command)
        os.system(wav_command)
        for container, codecs in formats.items():
            for codec in codecs:
                encoded_filename = "{}-{}ch-{}-{}.{}".format(
                    name, channel_count, rate, codec["codec"], codec["extension"]
                )
                print(encoded_filename)
                encoded_command = "ffmpeg -y -i {} -acodec {} {}".format(
                    wav_filename, codec["codec"], encoded_filename
                )
                print(encoded_command)
                os.system(encoded_command)
