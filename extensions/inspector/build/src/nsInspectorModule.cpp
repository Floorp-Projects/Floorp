/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

#include "nsIGenericFactory.h"

#include "inDOMRDFResource.h"
#include "inDOMDataSource.h"
#include "inDeepTreeWalker.h"
#include "inFlasher.h"
#include "inCSSValueSearch.h"
#include "inFileSearch.h"
#include "inDOMUtils.h"
#include "inBitmap.h"
#include "inBitmapDepot.h"
#include "inBitmapDecoder.h"
#include "inBitmapProtocolHandler.h"
#include "inPNGEncoder.h"

#ifdef WIN32
  #include "inScreenCapturer.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(inDOMDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(inDOMRDFResource)
NS_GENERIC_FACTORY_CONSTRUCTOR(inDeepTreeWalker)
NS_GENERIC_FACTORY_CONSTRUCTOR(inFlasher)
NS_GENERIC_FACTORY_CONSTRUCTOR(inCSSValueSearch)
NS_GENERIC_FACTORY_CONSTRUCTOR(inFileSearch)
NS_GENERIC_FACTORY_CONSTRUCTOR(inDOMUtils)
NS_GENERIC_FACTORY_CONSTRUCTOR(inBitmap)
NS_GENERIC_FACTORY_CONSTRUCTOR(inBitmapDepot)
NS_GENERIC_FACTORY_CONSTRUCTOR(inBitmapDecoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(inBitmapProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(inPNGEncoder)

#ifdef WIN32
  NS_GENERIC_FACTORY_CONSTRUCTOR(inScreenCapturer)
#endif

// {3AD0C13B-195E-48e7-AF29-F024E2D759EA}
#define IN_INSDOMDATASOURCE_CID \
{ 0x3ad0c13b, 0x195e, 0x48e7, { 0xaf, 0x29, 0xf0, 0x24, 0xe2, 0xd7, 0x59, 0xea } }

// {76C9161F-4C4C-4c1b-8F96-64587315C79E}
#define IN_DOMRDFRESOURCE_CID \
{ 0x76c9161f, 0x4c4c, 0x4c1b, { 0x8f, 0x96, 0x64, 0x58, 0x73, 0x15, 0xc7, 0x9e } }

// {BFCB82C2-5611-4318-90D6-BAF4A7864252}
#define IN_DEEPTREEWALKER_CID \
{ 0xbfcb82c2, 0x5611, 0x4318, { 0x90, 0xd6, 0xba, 0xf4, 0xa7, 0x86, 0x42, 0x52 } }

// {4D977F60-FBE7-4583-8CB7-F5ED882293EF}
#define IN_CSSVALUESEARCH_CID \
{ 0x4d977f60, 0xfbe7, 0x4583, { 0x8c, 0xb7, 0xf5, 0xed, 0x88, 0x22, 0x93, 0xef } }

// {9286E71A-621A-4b91-851E-9984C1A2E81A}
#define IN_FLASHER_CID \
{ 0x9286e71a, 0x621a, 0x4b91, { 0x85, 0x1e, 0x99, 0x84, 0xc1, 0xa2, 0xe8, 0x1a } }

// {D5636476-9F94-47f2-9CE9-69CDD9D7BBCD}
#define IN_FILESEARCH_CID \
{ 0xd5636476, 0x9f94, 0x47f2, { 0x9c, 0xe9, 0x69, 0xcd, 0xd9, 0xd7, 0xbb, 0xcd } }

// {40B22006-5DD5-42f2-BFE7-7DBF0757AB8B}
#define IN_DOMUTILS_CID \
{ 0x40b22006, 0x5dd5, 0x42f2, { 0xbf, 0xe7, 0x7d, 0xbf, 0x7, 0x57, 0xab, 0x8b } }

// {C4E47704-4C71-43f7-A37C-EF9FCD9ABBC2}
#define IN_BITMAP_CID \
{ 0xc4e47704, 0x4c71, 0x43f7, { 0xa3, 0x7c, 0xef, 0x9f, 0xcd, 0x9a, 0xbb, 0xc2 } }

// {99B8BA1F-B6B2-40ed-8BA4-F5EBC9BC1F10}
#define IN_BITMAPDEPOT_CID \
{ 0x99b8ba1f, 0xb6b2, 0x40ed, { 0x8b, 0xa4, 0xf5, 0xeb, 0xc9, 0xbc, 0x1f, 0x10 } }

// {9ED21085-3122-45bf-A275-61228EC5B8F2}
#define IN_BITMAPDECODER_CID \
{ 0x9ed21085, 0x3122, 0x45bf, { 0xa2, 0x75, 0x61, 0x22, 0x8e, 0xc5, 0xb8, 0xf2 } }

// {D08C5593-6C55-4d69-A0E9-4848D88CDCDA}
#define IN_BITMAPPROTOCOLHANDLER_CID \
{ 0xd08c5593, 0x6c55, 0x4d69, { 0xa0, 0xe9, 0x48, 0x48, 0xd8, 0x8c, 0xdc, 0xda } }

// {243533EA-446E-4a7e-A27A-C27890417DEE}
#define IN_PNGENCODER_CID \
{ 0x243533ea, 0x446e, 0x4a7e, { 0xa2, 0x7a, 0xc2, 0x78, 0x90, 0x41, 0x7d, 0xee } }

#ifdef WIN32
// {ECE14DF2-FC83-469f-83AC-684D5F06B6CE}
#define IN_SCREENCAPTURER_CID \
{ 0xece14df2, 0xfc83, 0x469f, { 0x83, 0xac, 0x68, 0x4d, 0x5f, 0x6, 0xb6, 0xce } }
#endif

static nsModuleComponentInfo components[] =
{
  { "DOM Datasource", 
    IN_INSDOMDATASOURCE_CID, 
    NS_RDF_DATASOURCE_CONTRACTID_PREFIX IN_DOMDATASOURCE_ID, 
    inDOMDataSourceConstructor },

  { "DOM RDF Resource", 
    IN_DOMRDFRESOURCE_CID, 
    NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX IN_DOMRDFRESOURCE_ID, 
    inDOMRDFResourceConstructor },

  { "Deep Tree Holder", 
    IN_DEEPTREEWALKER_CID, 
    "@mozilla.org/inspector/deep-tree-walker;1",
    inDeepTreeWalkerConstructor },

  { "Flasher", 
    IN_FLASHER_CID, 
    "@mozilla.org/inspector/flasher;1", 
    inFlasherConstructor },

  { "CSS Value Search", 
    IN_CSSVALUESEARCH_CID, 
    "@mozilla.org/inspector/search;1?type=cssvalue", 
    inCSSValueSearchConstructor },

  { "File Search", 
    IN_FILESEARCH_CID, 
    "@mozilla.org/inspector/search;1?type=file", 
    inFileSearchConstructor },

  { "DOM Utils", 
    IN_DOMUTILS_CID, 
    "@mozilla.org/inspector/dom-utils;1", 
    inDOMUtilsConstructor },

#ifdef WIN32
  { "Screen Capturer", 
    IN_SCREENCAPTURER_CID, 
    "@mozilla.org/inspector/screen-capturer;1",
    inScreenCapturerConstructor },
#endif

  { "Bitmap",
    IN_BITMAP_CID, 
    "@mozilla.org/inspector/bitmap;1",
    inBitmapConstructor },

  { "Bitmap Depot",
    IN_BITMAPDEPOT_CID, 
    "@mozilla.org/inspector/bitmap-depot;1",
    inBitmapDepotConstructor },

  { "Bitmap Channel",
    IN_BITMAPDECODER_CID, 
    "@mozilla.org/image/decoder;2?type=image/inspector-bitmap",
    inBitmapDecoderConstructor },

  { "Bitmap Protocol Handler",
    IN_BITMAPPROTOCOLHANDLER_CID,
    NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-bitmap",
    inBitmapProtocolHandlerConstructor },

  { "PNG Encoder",
    IN_PNGENCODER_CID,
    "@mozilla.org/inspector/png-encoder;1",
    inPNGEncoderConstructor }
};


NS_IMPL_NSGETMODULE(nsInspectorModule, components)
