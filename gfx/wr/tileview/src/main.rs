/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use webrender::{TileNode, TileNodeKind, InvalidationReason, TileOffset};
use webrender::{TileSerializer, TileCacheInstanceSerializer, TileCacheLoggerUpdateLists};
use serde::Deserialize;
//use ron::de::from_reader;
use std::fs::File;
use std::io::prelude::*;
use std::path::Path;
use std::ffi::OsString;
use webrender::api::enumerate_interners;
use webrender::UpdateKind;

#[derive(Deserialize)]
pub struct Slice {
    pub x: f32,
    pub y: f32,
    pub tile_cache: TileCacheInstanceSerializer
}

// invalidation reason CSS colors
static CSS_FRACTIONAL_OFFSET: &str       = "fill:#4040c0;fill-opacity:0.1;";
static CSS_BACKGROUND_COLOR: &str        = "fill:#10c070;fill-opacity:0.1;";
static CSS_SURFACE_OPACITY_CHANNEL: &str = "fill:#c040c0;fill-opacity:0.1;";
static CSS_NO_TEXTURE: &str              = "fill:#c04040;fill-opacity:0.1;";
static CSS_NO_SURFACE: &str              = "fill:#40c040;fill-opacity:0.1;";
static CSS_PRIM_COUNT: &str              = "fill:#40f0f0;fill-opacity:0.1;";
static CSS_CONTENT: &str                 = "fill:#f04040;fill-opacity:0.1;";

fn tile_node_to_svg(node: &TileNode, x: f32, y: f32) -> String
{
    match &node.kind {
        TileNodeKind::Leaf { .. } => {
            format!("<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" />\n",
                    (node.rect.origin.x + x) as i32,
                    (node.rect.origin.y + y) as i32,
                    node.rect.size.width, node.rect.size.height)
        },
        TileNodeKind::Node { children } => {
            children.iter().fold(String::new(), |acc, child| acc + &tile_node_to_svg(child, x, y) )
        }
    }
}

fn tile_to_svg(key: TileOffset,
               tile: &TileSerializer,
               slice: &Slice,
               prev_tile: Option<&TileSerializer>,
               tile_stroke: &str,
               prim_class: &str,
               invalidation_report: &mut String,
               svg_width: &mut i32, svg_height: &mut i32 ) -> String
{
    let mut svg = format!("\n<!-- tile key {},{} ; slice x {} y {}-->\n", key.x, key.y, slice.x, slice.y);


    let tile_fill =
        match tile.invalidation_reason {
            Some(InvalidationReason::FractionalOffset) => CSS_FRACTIONAL_OFFSET.to_string(),
            Some(InvalidationReason::BackgroundColor) => CSS_BACKGROUND_COLOR.to_string(),
            Some(InvalidationReason::SurfaceOpacityChanged) => CSS_SURFACE_OPACITY_CHANNEL.to_string(),
            Some(InvalidationReason::NoTexture) => CSS_NO_TEXTURE.to_string(),
            Some(InvalidationReason::NoSurface) => CSS_NO_SURFACE.to_string(),
            Some(InvalidationReason::PrimCount) => CSS_PRIM_COUNT.to_string(),
            Some(InvalidationReason::Content { prim_compare_result } ) => {
                let _foo = prim_compare_result;
                CSS_CONTENT.to_string() //TODO do something with the compare result
            }
            None => {
                let mut background = tile.background_color;
                if background.is_none() {
                    background = slice.tile_cache.background_color
                }
                match background {
                   Some(color) => {
                       let rgb = ( (color.r * 255.0) as u8,
                                   (color.g * 255.0) as u8,
                                   (color.b * 255.0) as u8 );
                       format!("fill:rgb({},{},{});fill-opacity:0.3;", rgb.0, rgb.1, rgb.2)
                   }
                   None => "fill:none;".to_string()
                }
            }
        };

    //let tile_style = format!("{}{}", tile_fill, tile_stroke);
    let tile_style = format!("{}stroke:none;", tile_fill);

    let title = match tile.invalidation_reason {
        Some(_) => format!("<title>slice {} tile ({},{}) - {:?}</title>",
                            slice.tile_cache.slice, key.x, key.y,
                            tile.invalidation_reason),
        None => String::new()
    };

    if let Some(reason) = tile.invalidation_reason {
        invalidation_report.push_str(
            &format!("\n<tspan x=\"0\" dy=\"16px\">slice {} tile key ({},{}) invalidated: {:?}</tspan>\n",
                     slice.tile_cache.slice, key.x, key.y, reason));
    }

    svg = format!(r#"{}<rect x="{}" y="{}" width="{}" height="{}" style="{}" ></rect>"#,
            svg,
            //TODO --bpe are these in local space or screen space?
            tile.rect.origin.x,
            tile.rect.origin.y,
            tile.rect.size.width,
            tile.rect.size.height,
            tile_style);

    svg = format!("{}\n\n<g class=\"svg_quadtree\">\n{}</g>\n",
                   svg,
                   //tile_node_to_svg(&tile.root, tile.rect.origin.x, tile.rect.origin.y));
                   tile_node_to_svg(&tile.root, 0.0, 0.0));

    let right  = (tile.rect.origin.x + tile.rect.size.width) as i32;
    let bottom = (tile.rect.origin.y + tile.rect.size.height) as i32;

    *svg_width  = if right  > *svg_width  { right  } else { *svg_width  };
    *svg_height = if bottom > *svg_height { bottom } else { *svg_height };

    svg += "\n<!-- primitives -->\n";

    svg = format!("{}<g id=\"{}\">\n\t",
                  svg,
                  prim_class);

    for prim in &tile.current_descriptor.prims {
        let rect = prim.prim_clip_rect;
        //TODO proper positioning of prims, especially when scrolling
        // this version seems closest, but introduces gaps (eg in about:config)
        let x = (rect.x + slice.x) as i32;
        let y = (rect.y + slice.y) as i32;
        // this version is .. interesting: when scrolling, nothing moves in about:config,
        // instead the searchbox shifts down.. hmm..
        //let x = rect.x as i32;
        //let y = rect.y as i32;
        let w = rect.w as i32;
        let h = rect.h as i32;

        let style =
            if let Some(prev_tile) = prev_tile {
                // when this O(n^2) gets too slow, stop brute-forcing and use a set or something
                if prev_tile.current_descriptor.prims.iter().find(|&prim| prim.prim_clip_rect == rect).is_some() {
                    ""
                } else {
                    "class=\"svg_changed_prim\" "
                }
            } else {
                "class=\"svg_changed_prim\" "
            };

        svg = format!(r#"{}<rect x="{}" y="{}" width="{}" height="{}" {}/>"#,
            svg,
            x, y, w, h,
            style);

        svg += "\n\t";
    }

    svg += "\n</g>\n";

    // nearly invisible, all we want is the toolip really
    let style = "style=\"fill-opacity:0.001;";
    svg += &format!("<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" {}{}\" >{}<\u{2f}rect>",
                    tile.rect.origin.x,
                    tile.rect.origin.y,
                    tile.rect.size.width,
                    tile.rect.size.height,
                    style,
                    tile_stroke,
                    title);

    svg
}

fn slices_to_svg(slices: &[Slice], prev_slices: Option<Vec<Slice>>,
                 svg_width: &mut i32, svg_height: &mut i32,
                 max_slice_index: &mut usize) -> String
{
    let svg_begin = "<?xml\u{2d}stylesheet type\u{3d}\"text/css\" href\u{3d}\"tilecache.css\" ?>\n";

    let mut svg = String::new();
    let mut invalidation_report = String::new();

    for slice in slices {
        let tile_cache = &slice.tile_cache;
        *max_slice_index = if tile_cache.slice > *max_slice_index { tile_cache.slice } else { *max_slice_index };

        let prim_class = format!("tile_slice{}", tile_cache.slice);

        //println!("slice {}", tile_cache.slice);
        svg.push_str(&format!("\n<!-- tile_cache slice {} -->\n",
                              tile_cache.slice));

        //let tile_stroke = "stroke:grey;stroke-width:1;".to_string();
        let tile_stroke = "stroke:none;".to_string();

        let mut prev_slice = None;
        if let Some(prev) = &prev_slices {
            for prev_search in prev {
                if prev_search.tile_cache.slice == tile_cache.slice {
                    prev_slice = Some(prev_search);
                    break;
                }
            }
        }

        for (key, tile) in &tile_cache.tiles {
            let mut prev_tile = None;
            if let Some(prev) = prev_slice {
                prev_tile = prev.tile_cache.tiles.get(key);
            }

            //println!("fofs  {:?}", tile.fract_offset);
            //println!("id    {:?}", tile.id);
            //println!("invr  {:?}", tile.invalidation_reason);
            svg.push_str(&tile_to_svg(*key, &tile, &slice, prev_tile, &tile_stroke, &prim_class,
                                      &mut invalidation_report, svg_width, svg_height));
        }
    }

    svg.push_str(&format!("<text x=\"0\" y=\"-8px\" class=\"svg_invalidated\">{}</text>\n", invalidation_report));

    format!(r#"{}<svg version="1.1" baseProfile="full" xmlns="http://www.w3.org/2000/svg" width="{}" height="{}" >"#,
                svg_begin,
                svg_width,
                svg_height)
            + "\n"
            + "<rect fill=\"black\" width=\"100%\" height=\"100%\"/>\n"
            + &svg
            + "\n</svg>\n"
}

fn write_html(output_dir: &Path, svg_files: &[String], intern_files: &[String]) {
    let html_head = "<!DOCTYPE html>\n\
                     <html>\n\
                     <head>\n\
                     <meta charset=\"UTF-8\">\n\
                     <link rel=\"stylesheet\" type=\"text/css\" href=\"tilecache.css\"></link>\n\
                     </head>\n"
                     .to_string();

    let html_body = "<body bgcolor=\"#000000\" onload=\"load()\">\n"
                     .to_string();


    let mut script = "\n<script>\n".to_string();

    script = format!("{}var svg_files = [\n", script);
    for svg_file in svg_files {
        script = format!("{}    \"{}\",\n", script, svg_file);
    }
    script = format!("{}];\n\n", script);

    script = format!("{}var intern_files = [\n", script);
    for intern_file in intern_files {
        script = format!("{}    \"{}\",\n", script, intern_file);
    }
    script = format!("{}];\n</script>\n\n", script);

    //TODO this requires copying the js file from somewhere?
    script = format!("{}<script src=\"tilecache.js\" type=\"text/javascript\"></script>\n\n", script);


    let html_end   = "</body>\n\
                      </html>\n"
                      .to_string();

    let html_body = format!(
        "{}\n\
        <div class=\"split left\">\n\
            <div>\n\
                <object id=\"svg_container0\" type=\"image/svg+xml\" data=\"{}\" class=\"tile_svg\" ></object>\n\
                <object id=\"svg_container1\" type=\"image/svg+xml\" data=\"{}\" class=\"tile_svg\" ></object>\n\
            </div>\n\
        </div>\n\
        \n\
        <div class=\"split right\">\n\
            <iframe width=\"100%\" id=\"intern\" src=\"{}\"></iframe>\n\
        </div>\n\
        \n\
        <div id=\"svg_ui_overlay\">\n\
            <div id=\"text_frame_counter\">{}</div>\n\
            <div id=\"text_spacebar\">Spacebar to Play</div>\n\
            <div>Use Left/Right to Step</div>\n\
            <input id=\"frame_slider\" type=\"range\" min=\"0\" max=\"{}\" value=\"0\" class=\"svg_ui_slider\" />
        </div>",
        html_body,
        svg_files[0],
        svg_files[0],
        intern_files[0],
        svg_files[0],
        svg_files.len() );

    let html = format!("{}{}{}{}", html_head, html_body, script, html_end);

    let output_file = output_dir.join("index.html");
    let mut html_output = File::create(output_file).unwrap();
    html_output.write_all(html.as_bytes()).unwrap();
}

fn write_css(output_dir: &Path, max_slice_index: usize) {
    let mut css = ".tile_svg {\n\
                    float: left;\n\
                   }\n\
                   \n\
                   .split {\n\
                      position: fixed;\n\
                      z-index: 1;\n\
                      top: 0;\n\
                      padding-top: 20px;\n\
                   }\n\
                   \n\
                   .left {\n\
                      left: 0;\n\
                   }\n\
                   \n\
                   .right {\n\
                      right: 0;\n\
                      width: 20%;\n\
                      height: 100%;\n\
                      opacity: 90%;\n\
                   }\n\
                   \n\
                   #intern {\n\
                    position:relative;\n\
                    top:60px;\n\
                    width: 100%;\n\
                    height: 100%;\n\
                    color: orange;\n\
                    background-color:white;\n\
                   }\n\
                   .svg_invalidated {\n\
                       fill: white;\n\
                       font-family:monospace;\n\
                   }\n\n\n\
                   #svg_ui_overlay {\n\
                       position:absolute;\n\
                       right:0; \n\
                       top:0; \n\
                       z-index:70; \n\
                       color: rgb(255,255,100);\n\
                       font-family:monospace;\n\
                       background-color: #404040a0;\n\
                   }\n\n\n\
                   .svg_quadtree {\n\
                       fill: none;\n\
                       stroke-width: 1;\n\
                       stroke: orange;\n\
                   }\n\n\n\
                   .svg_changed_prim {\n\
                       stroke: red;\n\
                       stroke-width: 2.0;\n\
                   }\n\n\n\
                   #svg_ui_slider {\n\
                       width:90%;\n\
                   }\n\n".to_string();

    for ix in 0..max_slice_index + 1 {
        let color = ( ix % 7 ) + 1;
        let rgb = format!("rgb({},{},{})",
                            if color & 2 != 0 { 205 } else { 90 },
                            if color & 4 != 0 { 205 } else { 90 },
                            if color & 1 != 0 { 225 } else { 90 });

        let prim_class = format!("tile_slice{}", ix);

        css = format!("{}\n\
                       #{} {{\n\
                           fill: {};\n\
                           fill-opacity: 0.03;\n\
                           stroke-width: 0.8;\n\
                           stroke: {};\n\
                       }}\n\n",
                       css,
                       prim_class,
                       //rgb,
                       "none",
                       rgb);
    }

    let output_file = output_dir.join("tilecache.css");
    let mut css_output = File::create(output_file).unwrap();
    css_output.write_all(css.as_bytes()).unwrap();
}

macro_rules! updatelist_to_html_macro {
    ( $( $name:ident: $ty:ty, )+ ) => {
        fn updatelist_to_html(update_lists: &TileCacheLoggerUpdateLists) -> String {
            let mut html = String::new();
            $(
                html += &format!("<h4 style=\"margin:5px;\">{}</h4>\n<font color=\"green\">\n", stringify!($name));
                let mut was_insert = true;
                for update in &update_lists.$name.1 {
                    let is_insert = match update.kind {
                        UpdateKind::Insert => true,
                        _ => false
                    };
                    if was_insert != is_insert {
                        html += &format!("</font><font color=\"{}\">", if is_insert { "green" } else { "red" });
                    }
                    html += &format!("{}, \n", update.index);
                    was_insert = is_insert;
                }
                html += &"</font><hr/>\n";
            )+
            html
        }
    }
}
enumerate_interners!(updatelist_to_html_macro);

fn write_tile_cache_visualizer_svg(entry: &std::fs::DirEntry, output_dir: &Path,
                                   slices: &[Slice], prev_slices: Option<Vec<Slice>>,
                                   svg_width: &mut i32, svg_height: &mut i32,
                                   max_slice_index: &mut usize,
                                   svg_files: &mut Vec::<String>)
{
    let svg = slices_to_svg(&slices, prev_slices, svg_width, svg_height, max_slice_index);

    let mut output_filename = OsString::from(entry.path().file_name().unwrap());
    output_filename.push(".svg");
    svg_files.push(output_filename.to_string_lossy().to_string());

    output_filename = output_dir.join(output_filename).into_os_string();
    let mut svg_output = File::create(output_filename).unwrap();
    svg_output.write_all(svg.as_bytes()).unwrap();
}

fn write_update_list_html(entry: &std::fs::DirEntry, output_dir: &Path,
                          update_lists: &TileCacheLoggerUpdateLists,
                          html_files: &mut Vec::<String>)
{
    let html = updatelist_to_html(update_lists);

    let mut output_filename = OsString::from(entry.path().file_name().unwrap());
    output_filename.push(".html");
    html_files.push(output_filename.to_string_lossy().to_string());

    output_filename = output_dir.join(output_filename).into_os_string();
    let mut html_output = File::create(output_filename).unwrap();
    html_output.write_all(html.as_bytes()).unwrap();
}

fn main() {
    let args: Vec<String> = std::env::args().collect();

    if args.len() != 3 {
        println!("Usage: tileview input_dir output_dir");
        println!("    where input_dir is a tile_cache folder inside a wr-capture.");
        println!("\nexample: cargo run c:/Users/me/AppData/Local/wr-capture.6/tile_cache/ c:/temp/tilecache/");
        std::process::exit(1);
    }

    let input_dir = Path::new(&args[1]);
    let output_dir = Path::new(&args[2]);
    std::fs::create_dir_all(output_dir).unwrap();

    let mut svg_width = 100i32;
    let mut svg_height = 100i32;
    let mut max_slice_index = 0;

    let mut entries: Vec<_> = std::fs::read_dir(input_dir).unwrap()
                                                          //.map(|r| r.unwrap())
                                                          .filter_map(|r| r.ok())
                                                          .collect();
    entries.sort_by_key(|dir| dir.path());

    let mut svg_files: Vec::<String> = Vec::new();
    let mut intern_files: Vec::<String> = Vec::new();
    let mut prev_slices = None;

    for entry in &entries {
        if entry.path().is_dir() {
            continue;
        }
        print!("processing {:?}\t", entry.path());
        let file_data = std::fs::read_to_string(entry.path()).unwrap();
        let chunks: Vec<_> = file_data.split("// @@@ chunk @@@").collect();
        let slices: Vec<Slice> = match ron::de::from_str(&chunks[0]) {
            Ok(data) => { data }
            Err(e) => {
                println!("ERROR: failed to deserialize slicesg {:?}\n{:?}", entry.path(), e);
                prev_slices = None;
                continue;
            }
        };
        let mut update_lists = TileCacheLoggerUpdateLists::new();
        update_lists.from_ron(&chunks[1]);

        write_tile_cache_visualizer_svg(&entry, &output_dir,
                                        &slices, prev_slices,
                                        &mut svg_width, &mut svg_height,
                                        &mut max_slice_index,
                                        &mut svg_files);

        write_update_list_html(&entry, &output_dir, &update_lists,
                               &mut intern_files);

        print!("\r");
        prev_slices = Some(slices);
    }

    write_html(output_dir, &svg_files, &intern_files);
    write_css(output_dir, max_slice_index);

    println!("OK. For now, manually copy tilecache.js to the output folder please.                           ");
}
