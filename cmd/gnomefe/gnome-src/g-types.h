
#ifndef _moz_types_h
#define _moz_types_h

typedef struct _MozTagged MozTagged;

typedef struct _MozComponent MozComponent;

typedef struct _MozView MozView;
typedef struct _MozPanedView MozPanedView;
typedef struct _MozHTMLView MozHTMLView;
typedef struct _MozEditorView MozEditorView;
typedef struct _MozBookmarkView MozBookmarkView;
typedef struct _MozHistoryView MozHistoryView;
typedef struct _MozNavCenterView MozNavCenterView;

typedef struct _MozFrame MozFrame;
typedef struct _MozBrowserFrame MozBrowserFrame;
typedef struct _MozEditorFrame MozEditorFrame;
typedef struct _MozBookmarkFrame MozBookmarkFrame;
typedef struct _MozHistoryFrame MozHistoryFrame;


/* tags for structures. */

#define MOZ_TAG_TAGGED			0x0001	/* 00000000 00000001 */
#define MOZ_TAG_COMPONENT		0x0003  /* 00000000 00000011 */
#define MOZ_TAG_VIEW			0x0007  /* 00000000 00000111 */
#define MOZ_TAG_FRAME			0x000D  /* 00000000 00001011 */

#define MOZ_TAG_PANED_VIEW		0x0017  /* 00000000 00010111 */
#define MOZ_TAG_HTML_VIEW		0x0027  /* 00000000 00100111 */
#define MOZ_TAG_EDITOR_VIEW		0x0067  /* 00000000 01100111 */
#define MOZ_TAG_BOOKMARK_VIEW		0x0087  /* 00000000 10000111 */
#define MOZ_TAG_HISTORY_VIEW		0x0107  /* 00000001 00000111 */
#define MOZ_TAG_NAVCENTER_VIEW		0x0207  /* 00000010 00000111 */

#define MOZ_TAG_BROWSER_FRAME		0x001D  /* 00000000 00011011 */
#define MOZ_TAG_EDITOR_FRAME		0x002D  /* 00000000 00101011 */
#define MOZ_TAG_BOOKMARK_FRAME		0x004D  /* 00000000 01001011 */
#define MOZ_TAG_HISTORY_FRAME		0x008D  /* 00000000 10001011 */

/* cast macros */

#if 1
/* these assert, for debugging */
#define MOZ_TAGGED(p) ((MozTagged*)p)
#define MOZ_COMPONENT(p) ((MozComponent*)p)
#define MOZ_VIEW(p) ((MozView*)p)
#define MOZ_FRAME(p) ((MozFrame*)p)
#define MOZ_HTML_VIEW(p) ((MozHTMLView*)p)
#define MOZ_BROWSER_FRAME(p) ((MozBrowserFrame*)p)
#define MOZ_BOOKMARK_FRAME(p) ((MozBookmarkFrame*)p)
#else
/* these don't do asserts -- no safety net here. */
#endif

#define MOZ_TAG_COMPARE(p, t)	 (MOZ_TAGGED((p))->tag & (t))
#define MOZ_IS_TAGGED(p)	 (MOZ_TAG_COMPARE(MOZ_TAGGED((p)), MOZ_TAG_TAGGED))
#define MOZ_IS_COMPONENT(p)	 (MOZ_TAG_COMPARE(MOZ_TAGGED((p)), MOZ_TAG_COMPONENT))
#define MOZ_IS_VIEW(p)		 (MOZ_TAG_COMPARE(MOZ_TAGGED((p)), MOZ_TAG_VIEW))
#define MOZ_IS_FRAME(p)		 (MOZ_TAG_COMPARE(MOZ_TAGGED((p)), MOZ_TAG_FRAME))
#define MOZ_IS_HTML_VIEW(p)	 (MOZ_TAG_COMPARE(MOZ_TAGGED((p)), MOZ_TAG_HTML_VIEW))
#define MOZ_IS_BROWSER_FRAME(p)	 (MOZ_TAG_COMPARE(MOZ_TAGGED((p)), MOZ_TAG_BROWSER_FRAME))
#define MOZ_IS_BOOKMARK_FRAME(p) (MOZ_TAG_COMPARE(MOZ_TAGGED((p)), MOZ_TAG_BOOKMARK_FRAME))

#endif /* _moz_types_h */
