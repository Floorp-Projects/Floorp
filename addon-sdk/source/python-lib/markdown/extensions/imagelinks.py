"""
========================= IMAGE LINKS =================================


Turns paragraphs like

<~~~~~~~~~~~~~~~~~~~~~~~~
dir/subdir
dir/subdir
dir/subdir
~~~~~~~~~~~~~~
dir/subdir
dir/subdir
dir/subdir
~~~~~~~~~~~~~~~~~~~>

Into mini-photo galleries.

"""

import re, markdown
import url_manager


IMAGE_LINK = """<a href="%s"><img src="%s" title="%s"/></a>"""
SLIDESHOW_LINK = """<a href="%s" target="_blank">[slideshow]</a>"""
ALBUM_LINK = """&nbsp;<a href="%s">[%s]</a>"""


class ImageLinksExtension(markdown.Extension):

    def extendMarkdown(self, md, md_globals):

        md.preprocessors.add("imagelink", ImageLinkPreprocessor(md), "_begin")


class ImageLinkPreprocessor(markdown.preprocessors.Preprocessor):

    def run(self, lines):

        url = url_manager.BlogEntryUrl(url_manager.BlogUrl("all"),
                                       "2006/08/29/the_rest_of_our")


        all_images = []
        blocks = []
        in_image_block = False

        new_lines = []
        
        for line in lines:

            if line.startswith("<~~~~~~~"):
                albums = []
                rows = []
                in_image_block = True

            if not in_image_block:

                new_lines.append(line)

            else:

                line = line.strip()
                
                if line.endswith("~~~~~~>") or not line:
                    in_image_block = False
                    new_block = "<div><br/><center><span class='image-links'>\n"

                    album_url_hash = {}

                    for row in rows:
                        for photo_url, title in row:
                            new_block += "&nbsp;"
                            new_block += IMAGE_LINK % (photo_url,
                                                       photo_url.get_thumbnail(),
                                                       title)
                            
                            album_url_hash[str(photo_url.get_album())] = 1
                        
                    new_block += "<br/>"
                            
                    new_block += "</span>"
                    new_block += SLIDESHOW_LINK % url.get_slideshow()

                    album_urls = album_url_hash.keys()
                    album_urls.sort()

                    if len(album_urls) == 1:
                        new_block += ALBUM_LINK % (album_urls[0], "complete album")
                    else :
                        for i in range(len(album_urls)) :
                            new_block += ALBUM_LINK % (album_urls[i],
                                                       "album %d" % (i + 1) )
                    
                    new_lines.append(new_block + "</center><br/></div>")

                elif line[1:6] == "~~~~~" :
                    rows.append([])  # start a new row
                else :
                    parts = line.split()
                    line = parts[0]
                    title = " ".join(parts[1:])

                    album, photo = line.split("/")
                    photo_url = url.get_photo(album, photo,
                                              len(all_images)+1)
                    all_images.append(photo_url)                        
                    rows[-1].append((photo_url, title))

                    if not album in albums :
                        albums.append(album)

        return new_lines


def makeExtension(configs):
    return ImageLinksExtension(configs)

