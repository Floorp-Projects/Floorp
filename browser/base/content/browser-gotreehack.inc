  <!-- Total hack. Used by the Go menu to build up items. -->
  <tree id="hiddenHistoryTree" hidden="true" collapsed="true" datasources="rdf:null"
        flags="dont-build-content"
        hidecolumnpicker="true">
    <template>
      <rule>
        <treechildren>
          <treeitem uri="rdf:*" rdf:type="rdf:http://www.w3.org/1999/02/22-rdf-syntax-ns#type">
            <treerow>
              <treecell label="rdf:http://home.netscape.com/NC-rdf#Name"/>
              <treecell label="rdf:http://home.netscape.com/NC-rdf#Date" hidden="true"/>
            </treerow>
          </treeitem>
        </treechildren>
      </rule>
    </template>
    <treecols id="hiddenHistoryTreeCols">
      <treecol flex="1" id="Name"
               hideheader="true" primary="true"/>
      <treecol id="Date" hidden="true" hideheader="true"
               sort="rdf:http://home.netscape.com/NC-rdf#Date" sortActive="true" sortDirection="descending"/>
    </treecols>
  </tree>
