# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Places Organizer Query Builder.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Annie Sullivan <annie.sullivan@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** 

<vbox id="advancedSearch">
  <hbox>
    <label value="&advancedSearch.match.label;"/> 
    <menulist id="advancedSearchType"
              oncommand="PlacesQueryBuilder.doSearch();">
      <menupopup>
        <menuitem value="and" label="&advancedSearch.all.label;"/>
        <menuitem value="or" label="&advancedSearch.any.label;"/>
      </menupopup>
    </menulist>
    <label value="&advancedSearch.rules.label;"/>
  </hbox>
  <grid flex="1">
    <columns>
      <column flex="1"/>
      <column flex="1"/>
      <column flex="0"/>
      <column flex="0"/>
    </columns>
    <rows id="advancedSearchRows">
      <row id="advancedSearch0Row" flex="1" hidden="true">
        <menulist id="advancedSearch0Subject" flex="1">
          <menupopup>
            <menuitem value="keyword"
                      label="&advancedSearch.subj_keyword.label;"
                      selected="true"
                      rowid="1"
                      id="advancedSearch0SubjectKeyword"
                      oncommand="PlacesQueryBuilder.showSearch(this.getAttribute('rowid'), PlacesQueryBuilder._keywordSearch);"/>
            <menuitem value="visited"
                      label="&advancedSearch.subj_visited.label;"
                      rowid="1"
                      id="advancedSearch0SubjectVisited"
                      oncommand="PlacesQueryBuilder.showSearch(this.getAttribute('rowid'), PlacesQueryBuilder._timeSearch);"/>
            <menuitem value="location"
                      label="&advancedSearch.subj_location.label;"
                      rowid="1"
                      id="advancedSearch0SubjectLocation"
                      oncommand="PlacesQueryBuilder.showSearch(this.getAttribute('rowid'), PlacesQueryBuilder._locationSearch);"/>
          </menupopup>
        </menulist>

        <hbox flex="1" align="center">
          <menulist id="advancedSearch0LocationMenulist"
                    oncommand="PlacesQueryBuilder.doSearch();"
                    hidden="true">
            <menupopup>
              <menuitem id="advancedSearch0LocationMenuSelected"
                        value="startswith"
                        label="&advancedSearch.location_startswith.label;"
                        selected="true"/>
              <menuitem value="is"
                        label="&advancedSearch.location_is.label;"/>
              <menuitem value="onsite"
                        label="&advancedSearch.location_onsite.label;"/>
            </menupopup>
          </menulist>

          <menulist id="advancedSearch0TimeMenulist" hidden="true">
            <menupopup>
              <menuitem value="on"
                        id="advancedSearch0TimeDefault"
                        label="&advancedSearch.time_is.label;"
                        selected="true"
                        rowid="1"
                        oncommand="PlacesQueryBuilder.showSearch(this.getAttribute('rowid'), PlacesQueryBuilder._timeSearch);"/>
              <menuitem value="before"
                        label="&advancedSearch.time_before.label;"
                        rowid="1"
                        oncommand="PlacesQueryBuilder.showSearch(this.getAttribute('rowid'), PlacesQueryBuilder._timeSearch);"/>
              <menuitem value="after"
                        label="&advancedSearch.time_after.label;"
                        rowid="1"
                        oncommand="PlacesQueryBuilder.showSearch(this.getAttribute('rowid'), PlacesQueryBuilder._timeSearch);"/>
              <menuitem value="inLast"
                        label="&advancedSearch.time_inlast.label;"
                        rowid="1"
                        oncommand="PlacesQueryBuilder.showSearch(this.getAttribute('rowid'), PlacesQueryBuilder._timeInLastSearch);"/>
            </menupopup>
          </menulist>
          <textbox type="timed"
                   timeout="50"
                   id="advancedSearch0Textbox"
                   flex="1"
                   oncommand="PlacesQueryBuilder.doSearch();"/>

          <textbox type="timed"
                   timeout="50"
                   id="advancedSearch0TimePicker"
                   hidden="true" flex="1"
                   rowid="1"
                   oncommand="PlacesQueryBuilder.onDateTyped(event, this.getAttribute('rowid'));"
                   onclick="PlacesQueryBuilder.handleTimePickerClick(event, this.getAttribute('rowid'));"/>
          <popup id="advancedSearch0DatePopup"
                 onpopupshowing="this.setAttribute('showing', true);"
                 onpopuphidden="this.removeAttribute('showing');">
            <vbox id="advancedSearch0Calendar"
                  class="history-calendar"
                  rowid="1"
                  onselectionchanged="return PlacesQueryBuilder.onCalendarChanged(event, event.target.getAttribute('rowid'));"/>
          </popup>

          <menulist id="advancedSearch0TimeMenulist2"
                    oncommand="PlacesQueryBuilder.doSearch();"
                    hidden="true">
            <menupopup>
              <menuitem value="days"
                        label="&advancedSearch.last_days.label;"
                        selected="true"/>
              <menuitem value="weeks"
                        label="&advancedSearch.last_weeks.label;"/>
              <menuitem value="months"
                        label="&advancedSearch.last_months.label;"/>
            </menupopup>
          </menulist>

        </hbox>              
        <button id="advancedSearch0Minus"
                label="&advancedSearch.minus.label;"
                class="small"
                oncommand="PlacesQueryBuilder.removeRow(event.target.parentNode);"/>
        <button id="advancedSearch0Plus"
                label="&advancedSearch.plus.label;"
                class="small"
                command="OrganizerCommand_search:moreCriteria"/>
      </row>
    </rows>
  </grid>
</vbox>
