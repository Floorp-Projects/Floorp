/*
  g-util.c -- misc. utilities.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#include "g-tagged.h"
#include "g-view.h"
#include "g-util.h"
#include "gnomefe.h"

MozView *
moz_get_view_of_type(MozView *root,
		     PRUint32 type)
{
  /* if the passed view is what we want */
  if ((MOZ_TAGGED(root)->tag & type) == type)
    return root;
  else /* we need to loop through our children. */
    {
      GList *cur = root->subviews;

      while (cur)
	{
	  MozView *new_root = MOZ_VIEW(cur->data);
	  MozView *view = NULL;

	  if (new_root)
	    view = moz_get_view_of_type(new_root, type);
	  if (view) return view;

	  cur = cur->next;
	}

      return NULL;
    }
}

MozEditorView *
find_editor_view(MWContext *context)
{
  MozEditorView *view;
  MozTagged *tagged = (MozTagged*)context->fe.data->frame_or_view;
  MozView *root;

  if (MOZ_IS_FRAME(tagged))
    root = moz_frame_get_top_view(MOZ_FRAME(tagged));
  else
    root = MOZ_VIEW(tagged);

  view = (MozEditorView*)moz_get_view_of_type(root,
					      MOZ_TAG_EDITOR_VIEW);
}

MozHTMLView *
find_html_view(MWContext *context)
{
  MozHTMLView *view;
  MozTagged *tagged = (MozTagged*)context->fe.data->frame_or_view;
  MozView *root;

  if (MOZ_IS_FRAME(tagged))
    root = moz_frame_get_top_view(MOZ_FRAME(tagged));
  else
    root = MOZ_VIEW(tagged);

  view = (MozHTMLView*)moz_get_view_of_type(root,
                                            MOZ_TAG_HTML_VIEW);
}

MozBookmarkView *
find_bookmark_view(MWContext *context)
{
  MozBookmarkView *view;

  MozTagged *tagged = (MozTagged*)context->fe.data->frame_or_view;
  MozView *root;

  if (MOZ_IS_FRAME(tagged))
    root = moz_frame_get_top_view(MOZ_FRAME(tagged));
  else
    root = MOZ_VIEW(tagged);

  view = (MozBookmarkView*)moz_get_view_of_type(root,
						MOZ_TAG_BOOKMARK_VIEW);
}

MozFrame*
find_frame(MWContext *context)
{
  MozTagged *tagged = (MozTagged*)context->fe.data->frame_or_view;

  if (MOZ_IS_FRAME(tagged))
    return MOZ_FRAME(tagged);
  else
    return MOZ_VIEW(tagged)->parent_frame;
}
