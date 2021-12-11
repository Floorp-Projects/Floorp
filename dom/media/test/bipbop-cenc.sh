mkdir work.tmp

for r in 225w_175kbps 300_215kbps 300wp_227kbps 360w_253kbps 480_624kbps 480wp_663kbps 480_959kbps 480wp_1001kbps
do
  for k in 1 2
  do
    # Encrypt bipbop_<res>.mp4 with the keys specified in this file,
    # and output to |bipbop_<res>-cenc-{video,audio}.mp4|
    MP4Box -crypt bipbop-cenc-audio-key${k}.xml -rem 1 -out work.tmp/bipbop_${r}-cenc-audio-key${k}.mp4 bipbop_${r}.mp4
    MP4Box -crypt bipbop-cenc-video-key${k}.xml -rem 2 -out work.tmp/bipbop_${r}-cenc-video-key${k}.mp4 bipbop_${r}.mp4

    # Fragment |bipbop_<res>-cenc-*.mp4| into 500ms segments:
    MP4Box -dash 500 -rap -segment-name work.tmp/bipbop_${r}-cenc-audio-key${k}- -subsegs-per-sidx 5 work.tmp/bipbop_${r}-cenc-audio-key${k}.mp4
    MP4Box -dash 500 -rap -segment-name work.tmp/bipbop_${r}-cenc-video-key${k}- -subsegs-per-sidx 5 work.tmp/bipbop_${r}-cenc-video-key${k}.mp4

    # The above command will generate a set of fragments |bipbop_<res>-cenc-{video,audio}-*.m4s
    # and |bipbop_<res>-cenc-{video,audio}-init.mp4| containing just the init segment.

    # Remove unneeded mpd files.
    rm bipbop_${r}-cenc-{audio,video}-key${k}_dash.mpd
  done
done

# Only keep the first 4 audio & 2 video segments:
cp work.tmp/*-init[.]mp4 ./
cp work.tmp/*audio*-[1234][.]m4s ./
cp work.tmp/*video*-[12][.]m4s ./

rm -Rf work.tmp
