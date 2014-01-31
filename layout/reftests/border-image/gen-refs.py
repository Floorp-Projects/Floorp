# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Generates tables of background images which correspond with border images for
# creating reftests. Input is the filename containing input defined below (a subset
# of the allowed CSS border properties). An html representation of a table is
# output to stdout.
#
# Usage: python gen-refs.py input_filename
#
# Input must take the form (order is not important, nothing is optional, distance in order top, right, bottom, left):
# width: p;
# height: p;
# border-width: p;
# border-image-source: ...;
# border-image-slice: p p p p;
# note that actually border-image-slice takes numbers without px, which represent pixels anyway (or at least coords)
# border-image-width: np np np np;
# border-image-repeat: stretch | repeat | round;
# border-image-outset: np np np np;
#
# where:
# p ::= n'px'
# np ::= n | p
#
# Assumes there is no intrinsic size for the border-image-source, so uses
# the size of the border image area.

import sys

class Point:
  def __init__(self, w=0, h=0):
    self.x = w
    self.y = h
class Size:
  def __init__(self, w=0, h=0):
    self.width = w
    self.height = h
class Rect:
  def __init__(self, x=0, y=0, x2=0, y2=0):
    self.x = x
    self.y = y
    self.x2 = x2
    self.y2 = y2
  def width(self):
    return self.x2 - self.x
  def height(self):
    return self.y2 - self.y

class Props:
  def __init__(self):
    self.size = Size()

class np:
  def __init__(self, n, p):
    self.n = n
    self.p = p

  def get_absolute(self, ref):
    if not self.p == 0:
      return self.p
    return self.n * ref

def parse_p(tok):
  if tok[-2:] == "px":
    return float(tok[:-2])
  print "Whoops, not a pixel value " + tok

def parse_np(tok):
  if tok[-2:] == "px":
    return np(0, float(tok[:-2]))
  return np(float(tok), 0)

def parse(filename):
  f = open(filename, "r")
  props = Props()
  for l in f:
    l = l.strip()
    if not l[-1] == ";":
      continue
    toks = l[:-1].split()
    if toks[0] == "border-width:":
      props.width = parse_p(toks[1])
    if toks[0] == "height:":
      props.size.height = parse_p(toks[1])
    if toks[0] == "width:":
      props.size.width = parse_p(toks[1])
    if toks[0] == "border-image-source:":
      props.source = l[l.find(":")+1:l.rfind(";")].strip()
    if toks[0] == "border-image-repeat:":
      props.repeat = toks[1]
    if toks[0] == "border-image-slice:":
      props.slice = map(parse_p, toks[1:5])
    if toks[0] == "border-image-width:":
      props.image_width = map(parse_np, toks[1:5])
    if toks[0] == "border-image-outset:":
      props.outset = map(parse_np, toks[1:5])
  f.close()
  return props

# the result of normalisation is that all sizes are in pixels and the size,
# widths, and outset have been normalised to a size and width - the former is
# the element's interior, the latter is the width of the drawn border.
def normalise(props):
  result = Props()
  result.source = props.source
  result.repeat = props.repeat
  result.width = map(lambda x: x.get_absolute(props.width), props.image_width)
  outsets = map(lambda x: x.get_absolute(props.width), props.outset)
  result.size.width = props.size.width + 2*props.width + outsets[1] + outsets[3]
  result.size.height = props.size.height + 2*props.width + outsets[0] + outsets[2]
  result.slice = props.slice
  for i in [0,2]:
    if result.slice[i] > result.size.height:
      result.slice[i] = result.size.height
    if result.slice[i+1] > result.size.width:
      result.slice[i+1] = result.size.width

  return result

def check_parse(props):
  if not hasattr(props, 'source'):
    print "missing border-image-source"
    return False
  if not hasattr(props.size, 'width'):
    print "missing width"
    return False
  if not hasattr(props.size, 'height'):
    print "missing height"
    return False
  if not hasattr(props, 'width'):
    print "missing border-width"
    return False
  if not hasattr(props, 'image_width'):
    print "missing border-image-width"
    return False
  if not hasattr(props, 'slice'):
    print "missing border-image-slice"
    return False
  if not hasattr(props, 'repeat') or (props.repeat not in ["stretch", "repeat", "round"]):
    print "missing or incorrect border-image-repeat '" + props.repeat + "'"
    return False
  if not hasattr(props, 'outset'):
    print "missing border-image-outset"
    return False

  return True

def check_normalise(props):
  if not hasattr(props, 'source'):
    print "missing border-image-source"
    return False
  if not hasattr(props.size, 'width'):
    print "missing width"
    return False
  if not hasattr(props.size, 'height'):
    print "missing height"
    return False
  if not hasattr(props, 'slice'):
    print "missing border-image-slice"
    return False
  if not hasattr(props, 'repeat') or (props.repeat not in ["stretch", "repeat", "round"]):
    print "missing or incorrect border-image-repeat '" + props.repeat + "'"
    return False

  return True

class Tile:
  def __init__(self):
    self.slice = Rect()
    self.border_width = Rect()

# throughout, we will use arrays for nine-patches, the indices correspond thusly:
# 0 1 2
# 3 4 5
# 6 7 8

# Compute the source tiles' slice and border-width sizes
def make_src_tiles():
  tiles = [Tile() for i in range(9)]

  rows = [range(3*i, 3*(i+1)) for i in range(3)]
  cols = [[i, i+3, i+6] for i in range(3)]

  row_limits_slice = [0, props.slice[3], props.size.width - props.slice[1], props.size.width]
  row_limits_width = [0, props.width[3], props.size.width - props.width[1], props.size.width]
  for r in range(3):
    for t in [tiles[i] for i in cols[r]]:
      t.slice.x = row_limits_slice[r]
      t.slice.x2 = row_limits_slice[r+1]
      t.border_width.x = row_limits_width[r]
      t.border_width.x2 = row_limits_width[r+1]

  col_limits_slice = [0, props.slice[0], props.size.height - props.slice[2], props.size.height]
  col_limits_width = [0, props.width[0], props.size.height - props.width[2], props.size.height]
  for c in range(3):
    for t in [tiles[i] for i in rows[c]]:
      t.slice.y = col_limits_slice[c]
      t.slice.y2 = col_limits_slice[c+1]
      t.border_width.y = col_limits_width[c]
      t.border_width.y2 = col_limits_width[c+1]

  return tiles

def compute(props):
  tiles = make_src_tiles()

  # corners scale easy
  for t in [tiles[i] for i in [0, 2, 6, 8]]:
    t.scale = Point(t.border_width.width()/t.slice.width(), t.border_width.height()/t.slice.height())
  # edges are by their secondary dimension
  for t in [tiles[i] for i in [1, 7]]:
    t.scale = Point(t.border_width.height()/t.slice.height(), t.border_width.height()/t.slice.height())
  for t in [tiles[i] for i in [3, 5]]:
    t.scale = Point(t.border_width.width()/t.slice.width(), t.border_width.width()/t.slice.width())
  # the middle is scaled by the factors for the top and left edges
  tiles[4].scale = Point(tiles[1].scale.x, tiles[3].scale.y)

  # the size of a source tile for the middle section
  src_tile_size = Size(tiles[4].slice.width()*tiles[4].scale.x, tiles[4].slice.height()*tiles[4].scale.y)

  # the size of a single destination tile in the central part
  dest_tile_size = Size()
  if props.repeat == "stretch":
    dest_tile_size.width = tiles[4].border_width.width()
    dest_tile_size.height = tiles[4].border_width.height()
    for t in [tiles[i] for i in [1, 7]]:
      t.scale.x = t.border_width.width()/t.slice.width()
    for t in [tiles[i] for i in [3, 5]]:
      t.scale.y = t.border_width.height()/t.slice.height()
  elif props.repeat == "repeat":
    dest_tile_size = src_tile_size
  elif props.repeat == "round":
    dest_tile_size.width = tiles[4].border_width.width() / math.ceil(tiles[4].border_width.width() / src_tile_size.width)
    dest_tile_size.height = tiles[4].border_width.height() / math.ceil(tiles[4].border_width.height() / src_tile_size.height)
    for t in [tiles[i] for i in [1, 4, 7]]:
      t.scale.x = dest_tile_size.width/t.slice.width()
    for t in [tiles[i] for i in [3, 4, 5]]:
      t.scale.y = dest_tile_size.height/t.slice.height()
  else:
    print "Whoops, invalid border-image-repeat value"

  # catch overlapping slices. Its easier to deal with it here than to catch
  # earlier and have to avoid all the divide by zeroes above
  for t in tiles:
    if t.slice.width() < 0:
      t.scale.x = 0
    if t.slice.height() < 0:
      t.scale.y = 0

  tiles_h = int(math.ceil(tiles[4].border_width.width()/dest_tile_size.width)+2)
  tiles_v = int(math.ceil(tiles[4].border_width.height()/dest_tile_size.height)+2)

  # if border-image-repeat: repeat, then we will later center the tiles, that
  # means we need an extra tile for the two 'half' tiles at either end
  if props.repeat == "repeat":
    if tiles_h % 2 == 0:
      tiles_h += 1
    if tiles_v % 2 == 0:
      tiles_v += 1
  dest_tiles = [Tile() for i in range(tiles_h * tiles_v)]

  # corners
  corners = [(0, 0), (tiles_h-1, 2), (tiles_v*(tiles_h-1), 6), (tiles_v*tiles_h-1, 8)]
  for d,s in corners:
    dest_tiles[d].size = Size(tiles[s].scale.x*props.size.width, tiles[s].scale.y*props.size.height)
    dest_tiles[d].dest_size = Size(tiles[s].border_width.width(), tiles[s].border_width.height())
  dest_tiles[0].offset                   = Point(0, 0)
  dest_tiles[tiles_h-1].offset           = Point(tiles[2].border_width.width() - dest_tiles[tiles_h-1].size.width, 0)
  dest_tiles[tiles_v*(tiles_h-1)].offset = Point(0, tiles[6].border_width.height() - dest_tiles[tiles_v*(tiles_h-1)].size.height)
  dest_tiles[tiles_v*tiles_h-1].offset   = Point(tiles[8].border_width.width() - dest_tiles[tiles_h*tiles_v-1].size.width, tiles[8].border_width.height() - dest_tiles[tiles_h*tiles_v-1].size.height)

  # horizontal edges
  for i in range(1, tiles_h-1):
    dest_tiles[i].size                       = Size(tiles[1].scale.x*props.size.width, tiles[1].scale.y*props.size.height)
    dest_tiles[(tiles_v-1)*tiles_h + i].size = Size(tiles[7].scale.x*props.size.width, tiles[7].scale.y*props.size.height)
    dest_tiles[i].dest_size                       = Size(dest_tile_size.width, tiles[1].border_width.height())
    dest_tiles[(tiles_v-1)*tiles_h + i].dest_size = Size(dest_tile_size.width, tiles[7].border_width.height())
    dest_tiles[i].offset                       = Point(-tiles[1].scale.x*tiles[1].slice.x, -tiles[1].scale.y*tiles[1].slice.y)
    dest_tiles[(tiles_v-1)*tiles_h + i].offset = Point(-tiles[7].scale.x*tiles[7].slice.x, -tiles[7].scale.y*tiles[7].slice.y)

  # vertical edges
  for i in range(1, tiles_v-1):
    dest_tiles[i*tiles_h].size       = Size(tiles[3].scale.x*props.size.width, tiles[3].scale.y*props.size.height)
    dest_tiles[(i+1)*tiles_h-1].size = Size(tiles[5].scale.x*props.size.width, tiles[5].scale.y*props.size.height)
    dest_tiles[i*tiles_h].dest_size       = Size(tiles[3].border_width.width(), dest_tile_size.height)
    dest_tiles[(i+1)*tiles_h-1].dest_size = Size(tiles[5].border_width.width(), dest_tile_size.height)
    dest_tiles[i*tiles_h].offset       = Point(-tiles[3].scale.x*tiles[3].slice.x, -tiles[3].scale.y*tiles[3].slice.y)
    dest_tiles[(i+1)*tiles_h-1].offset = Point(-tiles[5].scale.x*tiles[5].slice.x, -tiles[5].scale.y*tiles[5].slice.y)

  # middle
  for i in range(1, tiles_v-1):
    for j in range(1, tiles_h-1):
      dest_tiles[i*tiles_h+j].size = Size(tiles[4].scale.x*props.size.width, tiles[4].scale.y*props.size.height)
      dest_tiles[i*tiles_h+j].offset = Point(-tiles[4].scale.x*tiles[4].slice.x, -tiles[4].scale.y*tiles[4].slice.y)
      dest_tiles[i*tiles_h+j].dest_size = dest_tile_size

  # edge and middle tiles are centered with border-image-repeat: repeat
  # we need to change the offset to take account of this and change the dest_size
  # of the tiles at the sides of the edges if they are clipped
  if props.repeat == "repeat":
    diff_h = ((tiles_h-2)*dest_tile_size.width - tiles[4].border_width.width()) / 2
    diff_v = ((tiles_v-2)*dest_tile_size.height - tiles[4].border_width.height()) / 2
    for i in range(0, tiles_h):
      dest_tiles[tiles_h + i].dest_size.height -= diff_v
      dest_tiles[tiles_h + i].offset.y -= diff_v #* tiles[4].scale.y
      dest_tiles[(tiles_v-2)*tiles_h + i].dest_size.height -= diff_v
    for i in range(0, tiles_v):
      dest_tiles[i*tiles_h + 1].dest_size.width -= diff_h
      dest_tiles[i*tiles_h + 1].offset.x -= diff_h #* tiles[4].scale.x
      dest_tiles[(i+1)*tiles_h-2].dest_size.width -= diff_h

  # output the table to simulate the border
  print "<table>"
  for i in range(tiles_h):
    print "<col style=\"width: " + str(dest_tiles[i].dest_size.width) + "px;\">"
  for i in range(tiles_v):
    print "<tr style=\"height: " + str(dest_tiles[i*tiles_h].dest_size.height) + "px;\">"
    for j in range(tiles_h):
      width = dest_tiles[i*tiles_h+j].size.width
      height = dest_tiles[i*tiles_h+j].size.height
      # catch any tiles with negative widths/heights
      # this happends when the total of the border-image-slices > borde drawing area
      if width <= 0 or height <= 0:
        print "  <td style=\"background: white;\"></td>"
      else:
        print "  <td style=\"background-image: " + props.source + "; background-size: " + str(width) + "px " + str(height) + "px; background-position: " + str(dest_tiles[i*tiles_h+j].offset.x) + "px " + str(dest_tiles[i*tiles_h+j].offset.y) + "px;\"></td>"
    print "</tr>"
  print "</table>"


# start here
args = sys.argv[1:]
if len(args) == 0:
  print "whoops: no source file"
  exit(1)


props = parse(args[0])
if not check_parse(props):
  print dir(props)
  exit(1)
props = normalise(props)
if not check_normalise(props):
  exit(1)
compute(props)
